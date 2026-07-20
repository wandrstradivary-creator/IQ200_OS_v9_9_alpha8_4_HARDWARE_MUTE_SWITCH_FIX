#pragma once
#include <Arduino.h>
#include "RuntimeState.h"
#include "AudioEngine.h"
#include "WavPlayerService.h"
#include "FLACPlayerService.h"
#include "MP3PlayerService.h"
#include "MediaFramework.h"

// IQ200 OS v9.1.1 Next Track Engine
// Single player facade for local SD playback. The v9 contract is codec-neutral:
// SD -> decoder -> PCM16 stereo -> I2S -> PCM5102.
// This release keeps the proven real WAV PCM path active and exposes honest
// decoder readiness for FLAC/OPUS/MP3/OGG/AAC so UI/queue/runtime no longer
// depend on the legacy WAV-only command path.
class IQPlayerCore {
  RuntimeState* _rt = nullptr;
  AudioEngine* _audio = nullptr;
  WavPlayerService* _wav = nullptr;
  FLACPlayerService* _flac = nullptr;
  MP3PlayerService* _mp3 = nullptr;
  MediaCodec _lastCodec = MEDIA_CODEC_NONE;
  char _lastError[80] = "READY";
  bool _lastMirrorPlaying = false;

  static void copyText(char* dst, size_t len, const char* src) {
    if (!dst || len == 0) return;
    if (!src) src = "";
    strncpy(dst, src, len - 1);
    dst[len - 1] = 0;
  }

  void setPlayerStateName(const char* name) {
    if (!_rt) return;
    copyText(_rt->playerStateName, sizeof(_rt->playerStateName), name);
  }

  void clearPipelineMetrics() {
    if (!_rt) return;
    _rt->playerRingFillPct = 0;
    _rt->playerCacheHitPct = 0;
    _rt->playerReadAheadBytes = 0;
    _rt->playerDecoderLoadPct = 0;
  }

  bool markDecoderNotBuilt(MediaCodec codec, const char* path) {
    _lastCodec = codec;
    snprintf(_lastError, sizeof(_lastError), "%s decoder not built; use playwav to test PCM5102", MediaEngine::codecName(codec));
    if (_rt) {
      _rt->mediaCodec = codec;
      _rt->mediaState = MEDIA_STATE_READY;
      setPlayerStateName("READY");
      _rt->audioPlaying = false;
      _rt->rtAudioTaskRunning = false;
      _rt->audioBusy = false;
      _rt->wavPlayRequest = false;
      _rt->wavValid = false;
      _rt->wavProgress = 0;
      _rt->wavPlayedBytes = 0;
      _rt->mediaVuLeft = 0;
      _rt->mediaVuRight = 0;
      // v9.1.1: unsupported decoder must not erase a restored resume point.
      // Keep restored FLAC/OPUS/MP3/OGG/AAC position visible until the real
      // decoder starts updating it, while still reporting ERROR/decoder missing.
      if (_rt->resumeRestored && strncmp(_rt->resumeLastPath, path ? path : "", sizeof(_rt->resumeLastPath)) == 0 &&
          (_rt->resumeLoadedProgress > 0 || _rt->resumeLoadedPositionBytes > 0)) {
        _rt->mediaProgress = _rt->resumeLoadedProgress;
        _rt->mediaPlayedBytes = _rt->resumeLoadedPositionBytes;
      } else {
        _rt->mediaProgress = 0;
        _rt->mediaPlayedBytes = 0;
      }
      _lastMirrorPlaying = false;
      copyText(_rt->lastMessage, sizeof(_rt->lastMessage), _lastError);
      _rt->audioBusy = false;
      copyText(_rt->mediaPath, sizeof(_rt->mediaPath), path);
      copyText(_rt->mediaTitle, sizeof(_rt->mediaTitle), mediaTitleFromPath(path));
      _rt->uiDirty = true;
    }
    Serial.printf("[IQPLAYER] %s: %s\n", _lastError, path ? path : "");
    return false;
  }

public:
  bool begin(RuntimeState& rt, AudioEngine& audio, WavPlayerService& wav, FLACPlayerService& flac, MP3PlayerService& mp3) {
    _rt = &rt;
    _audio = &audio;
    _wav = &wav;
    _flac = &flac;
    _mp3 = &mp3;
    copyText(_lastError, sizeof(_lastError), "READY");
    _lastMirrorPlaying = false;
    return _wav->begin() && _flac->begin() && _mp3->begin();
  }

  bool play(const char* path) {
    if (!_rt || !_audio || !_wav || !_flac || !_mp3) return false;
    if (!path || !path[0]) {
      copyText(_lastError, sizeof(_lastError), "empty path");
      return false;
    }

    // v9.1.1: hard handoff guard. If a new play request arrives while a
    // decoder task is still active, stop both local decoders first. This avoids
    // FLAC continuing to stream when user hits next/prev and lands on an
    // unsupported codec such as MP3.
    if ((_wav && _wav->isPlaying()) || (_flac && _flac->isPlaying()) || (_mp3 && _mp3->isPlaying()) || _rt->rtAudioTaskRunning) {
      if (_wav) _wav->stop();
      if (_flac) _flac->stop();
      if (_mp3) _mp3->stop();
      delay(2);
    }

    MediaCodec codec = mediaCodecFromPath(path);
    _lastCodec = codec;
    _rt->mediaCodec = codec;
    _rt->mediaState = MEDIA_STATE_LOADING;
    setPlayerStateName("LOADING");
    _rt->audioBusy = true;
    copyText(_rt->mediaPath, sizeof(_rt->mediaPath), path);
    copyText(_rt->playlistCurrent, sizeof(_rt->playlistCurrent), path);
    copyText(_rt->mediaTitle, sizeof(_rt->mediaTitle), mediaTitleFromPath(path));
    _rt->uiDirty = true;


    if (codec == MEDIA_CODEC_FLAC) {
      Serial.printf("[IQPLAYER] FLAC SD->dr_flac->I2S start: %s\n", path);
      bool ok = _flac->open(path, *_audio);
      _rt->wavValid = false;
      _rt->audioPlaying = ok;
      _rt->rtAudioTaskRunning = ok;
      _rt->audioBusy = false;
      if (ok) {
        _rt->mediaCodec = MEDIA_CODEC_FLAC;
        _rt->mediaState = MEDIA_STATE_PLAYING;
        setPlayerStateName("PLAYING");
        _rt->mediaSampleRate = _flac->info().sampleRate;
        _rt->mediaChannels = 2;
        _rt->mediaBits = 16;
        _rt->mediaDataSize = _flac->total();
        _rt->mediaProgress = 0;
        _rt->mediaPlayedBytes = 0;
        copyText(_lastError, sizeof(_lastError), "OK");
        Serial.printf("[IQPLAYER] FLAC info: %lu Hz source_ch=%u out_ch=2 bits=16 pcm_bytes=%lu\n",
          (unsigned long)_flac->info().sampleRate, _flac->info().channels, (unsigned long)_flac->total());
      } else {
        _rt->mediaState = MEDIA_STATE_ERROR;
        setPlayerStateName("ERROR");
        _rt->audioPlaying = false;
        _rt->rtAudioTaskRunning = false;
        copyText(_lastError, sizeof(_lastError), "FLAC open/decode init failed");
        Serial.printf("[IQPLAYER] FLAC open/decode init failed: %s\n", path);
      }
      return ok;
    }

    if (codec == MEDIA_CODEC_MP3) {
      Serial.printf("[IQPLAYER] MP3 SD->dr_mp3->I2S start: %s\n", path);
      bool ok = _mp3->open(path, *_audio);
      _rt->wavValid = false;
      _rt->audioPlaying = ok;
      _rt->rtAudioTaskRunning = ok;
      _rt->audioBusy = false;
      if (ok) {
        _rt->mediaCodec = MEDIA_CODEC_MP3;
        _rt->mediaState = MEDIA_STATE_PLAYING;
        setPlayerStateName("PLAYING");
        _rt->mediaSampleRate = _mp3->info().sampleRate;
        _rt->mediaChannels = 2;
        _rt->mediaBits = 16;
        _rt->mediaDataSize = _mp3->total();
        _rt->mediaProgress = 0;
        _rt->mediaPlayedBytes = 0;
        copyText(_lastError, sizeof(_lastError), "OK");
        Serial.printf("[IQPLAYER] MP3 info: %lu Hz source_ch=%u out_ch=2 bits=16 pcm_bytes=%lu\n",
          (unsigned long)_mp3->info().sampleRate, _mp3->info().channels, (unsigned long)_mp3->total());
      } else {
        _rt->mediaState = MEDIA_STATE_ERROR;
        setPlayerStateName("ERROR");
        _rt->audioPlaying = false;
        _rt->rtAudioTaskRunning = false;
        copyText(_lastError, sizeof(_lastError), "MP3 open/decode init failed");
        Serial.printf("[IQPLAYER] MP3 open/decode init failed: %s\n", path);
      }
      return ok;
    }

    if (codec == MEDIA_CODEC_WAV) {
      Serial.printf("[IQPLAYER] WAV PCM16 SD->I2S start: %s\n", path);
      bool ok = _wav->open(path, *_audio);
      _rt->wavValid = ok;
      _rt->audioPlaying = ok;
      _rt->rtAudioTaskRunning = ok;
      _rt->audioBusy = false;
      if (ok) {
        _rt->wavSampleRate = _wav->info().sampleRate;
        _rt->wavChannels = _wav->info().channels;
        _rt->wavDataSize = _wav->info().dataSize;
        _rt->mediaCodec = MEDIA_CODEC_WAV;
        _rt->mediaState = MEDIA_STATE_PLAYING;
        setPlayerStateName("PLAYING");
        _rt->mediaSampleRate = _rt->wavSampleRate;
        _rt->mediaChannels = _rt->wavChannels;
        _rt->mediaBits = _wav->info().bitsPerSample;
        _rt->mediaDataSize = _rt->wavDataSize;
        copyText(_lastError, sizeof(_lastError), "OK");
        Serial.printf("[IQPLAYER] WAV info: %lu Hz ch=%u bits=%u data=%lu\n",
          (unsigned long)_wav->info().sampleRate, _wav->info().channels,
          _wav->info().bitsPerSample, (unsigned long)_wav->info().dataSize);
      } else {
        _rt->mediaState = MEDIA_STATE_ERROR;
        setPlayerStateName("ERROR");
        copyText(_lastError, sizeof(_lastError), "WAV open failed");
        Serial.printf("[IQPLAYER] WAV open failed: %s\n", path);
      }
      return ok;
    }

    _rt->audioBusy = false;
    if (codec == MEDIA_CODEC_OPUS || codec == MEDIA_CODEC_OGG || codec == MEDIA_CODEC_AAC) {
      return markDecoderNotBuilt(codec, path);
    }

    return markDecoderNotBuilt(MEDIA_CODEC_NONE, path);
  }

  void stop() {
    if (_wav) _wav->stop();
    if (_flac) _flac->stop();
    if (_mp3) _mp3->stop();
    if (_rt) {
      _rt->audioPlaying = false;
      _rt->rtAudioTaskRunning = false;
      _rt->mediaState = MEDIA_STATE_STOPPED;
      setPlayerStateName("STOPPED");
      clearPipelineMetrics();
      _rt->mediaProgress = 0;
      _rt->wavProgress = 0;
      _rt->wavPlayedBytes = 0;
      _rt->mediaVuLeft = 0;
      _rt->mediaVuRight = 0;
      _rt->wavVuLeft = 0;
      _rt->wavVuRight = 0;
      _rt->uiDirty = true;
    }
    copyText(_lastError, sizeof(_lastError), "STOPPED");
  }

  void mirror() {
    if (!_rt || !_wav) return;
    bool p = _wav->isPlaying();
    bool fp = _flac && _flac->isPlaying();
    bool mp = _mp3 && _mp3->isPlaying();
    if (_rt->mediaCodec == MEDIA_CODEC_MP3) {
      _rt->audioPlaying = mp;
      _rt->rtAudioTaskRunning = _mp3->isTaskRunning();
      _rt->audioUnderruns = _mp3->underrunCount();
      _rt->audioShortWrites = _mp3->shortWriteCount();
      _rt->audioRtLoops = _mp3->loopCount();
      _rt->audioLastChunkBytes = _mp3->lastChunk();
      _rt->audioTaskStackHighWater = _mp3->taskStackHighWater();
      _rt->audioHealth = _mp3->health();
      _rt->mediaPlayedBytes = _mp3->played();
      _rt->mediaProgress = _mp3->progressPercent();
      _rt->mediaVuLeft = _mp3->vuLeft();
      _rt->mediaVuRight = _mp3->vuRight();
      _rt->mediaBufferHealth = _rt->audioHealth;
      _rt->mediaUnderruns = _rt->audioUnderruns;
      _rt->mediaShortWrites = _rt->audioShortWrites;
      _rt->playerSdRetries = _mp3->retryCount();
      _rt->playerSdErrors = _mp3->sdErrorCount();
      if (mp) {
        setPlayerStateName("PLAYING");
        _rt->mediaState = MEDIA_STATE_PLAYING;
      } else if (_lastMirrorPlaying) {
        if (_mp3->failed()) {
          _rt->mediaState = MEDIA_STATE_ERROR;
          setPlayerStateName("SDERR");
          _rt->audioHealth = 0;
          _rt->mediaBufferHealth = 0;
          copyText(_rt->lastMessage, sizeof(_rt->lastMessage), _mp3->lastError());
          copyText(_lastError, sizeof(_lastError), _mp3->lastError());
        } else if (_mp3->finishedNaturally()) {
          _rt->mediaProgress = 100;
          _rt->mediaPlayedBytes = _rt->mediaDataSize;
          _rt->mediaState = MEDIA_STATE_STOPPED;
          setPlayerStateName("EOF");
          clearPipelineMetrics();
          copyText(_lastError, sizeof(_lastError), "EOF");
        } else {
          _rt->mediaState = MEDIA_STATE_STOPPED;
          setPlayerStateName("STOPPED");
          clearPipelineMetrics();
          copyText(_lastError, sizeof(_lastError), "STOPPED");
        }
        _rt->uiDirty = true;
      }
      _lastMirrorPlaying = mp;
      return;
    }
    if (_rt->mediaCodec == MEDIA_CODEC_FLAC) {
      _rt->audioPlaying = fp;
      _rt->rtAudioTaskRunning = _flac->isTaskRunning();
      _rt->audioUnderruns = _flac->underrunCount();
      _rt->audioShortWrites = _flac->shortWriteCount();
      _rt->audioRtLoops = _flac->loopCount();
      _rt->audioLastChunkBytes = _flac->lastChunk();
      _rt->audioTaskStackHighWater = _flac->taskStackHighWater();
      _rt->audioHealth = _flac->health();
      _rt->mediaPlayedBytes = _flac->played();
      _rt->mediaProgress = _flac->progressPercent();
      _rt->mediaVuLeft = _flac->vuLeft();
      _rt->mediaVuRight = _flac->vuRight();
      _rt->mediaBufferHealth = _rt->audioHealth;
      _rt->mediaUnderruns = _rt->audioUnderruns;
      _rt->mediaShortWrites = _rt->audioShortWrites;
      _rt->playerCacheHits = _flac->cacheHitCount();
      _rt->playerCacheMisses = _flac->cacheMissCount();
      uint32_t totalCache = _rt->playerCacheHits + _rt->playerCacheMisses;
      _rt->playerCacheHitPct = totalCache ? (uint8_t)((_rt->playerCacheHits * 100UL) / totalCache) : 100;
      _rt->playerReadAheadBytes = _flac->readAhead();
      _rt->playerSdRetries = _flac->retryCount();
      _rt->playerSdErrors = _flac->sdErrorCount();
      _rt->playerRingFillPct = _flac->ringFill();
      _rt->playerDecoderLoadPct = _flac->decoderLoad();
      if (fp) {
        setPlayerStateName("PLAYING");
        _rt->mediaState = MEDIA_STATE_PLAYING;
      } else if (_lastMirrorPlaying) {
        if (_flac->failed()) {
          // v9.1.1: SD/FAT read failure is not a normal EOF. Do not show 100%,
          // do not mark the track finished, and do not let SmartResume save a
          // bogus completion point. Keep the last good played/progress values.
          _rt->mediaState = MEDIA_STATE_ERROR;
          setPlayerStateName("SDERR");
          _rt->audioHealth = 0;
          _rt->mediaBufferHealth = 0;
          copyText(_rt->lastMessage, sizeof(_rt->lastMessage), _flac->lastError());
          copyText(_lastError, sizeof(_lastError), _flac->lastError());
        } else if (_flac->finishedNaturally()) {
          _rt->mediaProgress = 100;
          _rt->mediaPlayedBytes = _rt->mediaDataSize;
          _rt->mediaState = MEDIA_STATE_STOPPED;
          setPlayerStateName("EOF");
          clearPipelineMetrics();
          copyText(_lastError, sizeof(_lastError), "EOF");
        } else {
          _rt->mediaState = MEDIA_STATE_STOPPED;
          setPlayerStateName("STOPPED");
          clearPipelineMetrics();
          copyText(_lastError, sizeof(_lastError), "STOPPED");
        }
        _rt->uiDirty = true;
      }
      _lastMirrorPlaying = fp;
      return;
    }
    _rt->audioPlaying = p;
    _rt->rtAudioTaskRunning = _wav->isTaskRunning();
    _rt->wavPlayedBytes = _wav->played();
    _rt->wavProgress = _wav->progressPercent();
    _rt->wavVuLeft = _wav->vuLeft();
    _rt->wavVuRight = _wav->vuRight();
    _rt->audioUnderruns = _wav->underrunCount();
    _rt->audioShortWrites = _wav->shortWriteCount();
    _rt->audioRtLoops = _wav->loopCount();
    _rt->audioLastChunkBytes = _wav->lastChunk();
    _rt->audioTaskStackHighWater = _wav->taskStackHighWater();
    _rt->audioHealth = _wav->health();
    _rt->mediaPlayedBytes = _rt->wavPlayedBytes;
    if (p) {
      _rt->mediaProgress = _rt->wavProgress;
    } else if (_lastMirrorPlaying && _rt->wavValid) {
      // Natural end of WAV: keep a visible 100% completion until explicit stop
      // or next play request resets state. This also avoids autosave seeing 0%.
      _rt->wavProgress = 100;
      _rt->mediaProgress = 100;
      _rt->mediaPlayedBytes = _rt->mediaDataSize ? _rt->mediaDataSize : _rt->wavDataSize;
    } else {
      _rt->mediaProgress = _rt->wavProgress;
    }
    _rt->mediaVuLeft = _rt->wavVuLeft;
    _rt->mediaVuRight = _rt->wavVuRight;
    _rt->mediaBufferHealth = _rt->audioHealth;
    _rt->mediaUnderruns = _rt->audioUnderruns;
    _rt->mediaShortWrites = _rt->audioShortWrites;
    if (p) { _rt->mediaState = MEDIA_STATE_PLAYING; setPlayerStateName("PLAYING"); }
    else if (_lastMirrorPlaying) { setPlayerStateName("STOPPED"); clearPipelineMetrics(); }
    _lastMirrorPlaying = p;
  }

  bool isPlaying() const { return (_wav && _wav->isPlaying()) || (_flac && _flac->isPlaying()) || (_mp3 && _mp3->isPlaying()); }
  bool isTaskRunning() const {
    return (_wav && _wav->isTaskRunning()) || (_flac && _flac->isTaskRunning()) || (_mp3 && _mp3->isTaskRunning());
  }

  // v9.1.1: stop the active decoder and wait until its FreeRTOS task has
  // really exited before another file/decoder is opened.
  bool stopAndWait(uint32_t timeoutMs = 1200) {
    if (_wav) _wav->stop();
    if (_flac) _flac->stop();
    if (_mp3) _mp3->stop();
    const uint32_t started = millis();
    while (isTaskRunning()) {
      if ((uint32_t)(millis() - started) >= timeoutMs) {
        copyText(_lastError, sizeof(_lastError), "handoff stop timeout");
        return false;
      }
      vTaskDelay(pdMS_TO_TICKS(2));
    }
    if (_rt) {
      _rt->audioPlaying = false;
      _rt->rtAudioTaskRunning = false;
      _rt->audioBusy = false;
      _rt->mediaVuLeft = 0;
      _rt->mediaVuRight = 0;
      _rt->wavVuLeft = 0;
      _rt->wavVuRight = 0;
    }
    copyText(_lastError, sizeof(_lastError), "handoff stopped");
    return true;
  }
  const char* lastError() const { return _lastError; }
  const char* priority() const { return "FLAC -> MP3 -> WAV -> OPUS -> OGG -> AAC"; }
};
