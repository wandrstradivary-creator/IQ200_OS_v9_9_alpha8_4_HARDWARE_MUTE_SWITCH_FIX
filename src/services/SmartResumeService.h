#pragma once
#include <Arduino.h>
#include "RuntimeState.h"
#include "DatabaseService.h"
#include "SDManager.h"
#include "QueueManager.h"
#include "AudioEngine.h"
#include "MediaEngine.h"

// IQ200 OS v9.1.1 Smart Resume
// Low-write automatic resume coordinator. It keeps all persistent resume data
// on SD through DatabaseService/ResumeEngine and avoids blocking UI/encoder.
class SmartResumeService {
public:
  void begin(RuntimeState& state, DatabaseService& db, QueueManager& queue, AudioEngine& audio) {
    _rt = &state;
    _db = &db;
    _queue = &queue;
    _audio = &audio;
    _lastSaveMs = 0;
    _lastProgress = state.mediaProgress;
    _lastPlayed = state.mediaPlayedBytes;
    _lastQueueIndex = queue.index();
    _lastQueueCount = queue.size();
    _lastShuffle = queue.shuffleSmart();
    _lastRepeat = queue.repeatMode();
    _lastVolume = audio.getVolume();
    copyPath(_lastPath, sizeof(_lastPath), state.mediaPath);
  }

  bool restoreAtBoot() {
    if (!_rt || !_db || !_queue || !_audio || !_rt->resumeAutoEnabled) return false;
    _rt->resumeRestored = false;
    char beforePath[192];
    char beforeTitle[64];
    copyPath(beforePath, sizeof(beforePath), _rt->mediaPath);
    copyPath(beforeTitle, sizeof(beforeTitle), _rt->mediaTitle);
    uint8_t beforeCodec = _rt->mediaCodec;
    uint8_t beforeState = _rt->mediaState;
    bool qok = _db->loadQueue();
    bool rok = _db->loadResume();
    if (rok && strncmp(_rt->mediaPath, "/test.wav", 9) == 0) {
      // v9.1.1: /test.wav is a hardware verification path. Older builds could
      // accidentally persist it. Ignore it at boot so normal library resume is
      // not poisoned and the player does not boot into LOADING test state.
      copyPath(_rt->mediaPath, sizeof(_rt->mediaPath), beforePath);
      copyPath(_rt->mediaTitle, sizeof(_rt->mediaTitle), beforeTitle);
      _rt->mediaCodec = beforeCodec;
      _rt->mediaState = beforeState;
      _rt->mediaProgress = 0;
      _rt->mediaPlayedBytes = 0;
      _rt->resumeRestored = false;
      Serial.println("[SMARTRESUME] ignored test WAV resume entry; keeping library current track");
      rok = false;
    }
    if (qok) {
      _rt->queueCount = _queue->size();
      _rt->queueIndex = _queue->index();
      _rt->queueShuffleSmart = _queue->shuffleSmart();
      _rt->queueRepeatMode = _queue->repeatMode();
      String cur = _queue->current();
      if (cur.length()) copyPath(_rt->queueCurrent, sizeof(_rt->queueCurrent), cur.c_str());
    }
    if (rok) {
      _audio->setVolume(_rt->resumeLoadedVolume);
      _rt->volumePercent = _rt->resumeLoadedVolume;
      _rt->resumeRestored = true;
      _rt->resumeRestoreCount++;
      copyPath(_lastPath, sizeof(_lastPath), _rt->mediaPath);
      _lastProgress = _rt->mediaProgress;
      _lastPlayed = _rt->mediaPlayedBytes;
      _lastVolume = _audio->getVolume();
      _lastQueueIndex = _queue->index();
      _lastQueueCount = _queue->size();
      _lastShuffle = _queue->shuffleSmart();
      _lastRepeat = _queue->repeatMode();
      Serial.printf("[SMARTRESUME] restored track-only: path=%s playlist=%d/%d volume=%d repeat=%d\n",
                    _rt->mediaPath,
                    _rt->resumeLoadedPlaylistIndex >= 0 ? _rt->resumeLoadedPlaylistIndex + 1 : 0,
                    _rt->resumeLoadedPlaylistCount,
                    _audio->getVolume(), _queue->repeatMode());
    } else {
      Serial.println("[SMARTRESUME] no resume.dat; cold start");
    }
    return qok || rok;
  }

  void markDirty() { _dirty = true; }

  void tick() {
    if (!_rt || !_db || !_queue || !_audio || !_rt->resumeAutoEnabled) return;
    if (!_db->started()) return;
    _rt->volumePercent = _audio->getVolume();

    // v9.1.1: Resume guard. Decoder-not-built formats (currently FLAC/OPUS/MP3/OGG/AAC)
    // must not overwrite a valid restored resume position with 0% just because the
    // runtime is idle/READY and the WAV mirror has no active PCM stream. This keeps
    // the user's real library resume intact while playwav remains a hardware test.
    if (shouldHoldRestoredPosition()) {
      return;
    }

    // v9.1.1: Track-only resume. Do not save because progress/played changed.
    // Only selected track, playlist index/count, repeat, and volume changes can persist.
    bool changed = _dirty || pathChanged() ||
                   _queue->index() != _lastQueueIndex || _queue->size() != _lastQueueCount ||
                   _queue->shuffleSmart() != _lastShuffle || _queue->repeatMode() != _lastRepeat ||
                   _audio->getVolume() != _lastVolume;
    if (!changed) return;

    // v9.2-alpha7: FLAC owns the SD data path while its decoder task is active.
    // Do not write queue/resume files concurrently with FLAC reads. Keep the
    // dirty state intact; the normal autosave will run after playback stops.
    const bool flacOwnsSd = (_rt->mediaCodec == MEDIA_CODEC_FLAC) &&
                            (_rt->audioPlaying || _rt->rtAudioTaskRunning ||
                             _rt->mediaState == MEDIA_STATE_PLAYING);
    if (flacOwnsSd || SDManager::streamActive() || SDManager::recovering()) return;
    uint32_t now = millis();
    if ((uint32_t)(now - _lastSaveMs) < _minSaveIntervalMs && !_rt->resumeAutoSaveRequest) return;
    _rt->resumeAutoSaveRequest = false;
    if (saveNow()) {
      _dirty = false;
      _lastSaveMs = now;
      _rt->resumeLastAutoSaveMs = now;
      _lastProgress = _rt->mediaProgress;
      _lastPlayed = _rt->mediaPlayedBytes;
      _lastQueueIndex = _queue->index();
      _lastQueueCount = _queue->size();
      _lastShuffle = _queue->shuffleSmart();
      _lastRepeat = _queue->repeatMode();
      _lastVolume = _audio->getVolume();
      copyPath(_lastPath, sizeof(_lastPath), _rt->mediaPath);
    }
  }

  bool saveNow() {
    if (!_rt || !_db || !_queue || !_audio) return false;
    if (SDManager::streamActive() || SDManager::recovering()) return false;
    // v9.1.1: never persist the transient old/new path while an atomic
    // next/prev handoff is in progress.
    if (_rt->trackHandoffActive) return false;
    // v9.1.1: playwav is a hardware test path. Do not overwrite the user's
    // real resume target with /test.wav when validating PCM5102/I2S.
    if (strncmp(_rt->mediaPath, "/test.wav", 9) == 0) {
      return false;
    }
    // v9.1.1: any playback started via the explicit playwav hardware-test
    // command is not allowed to replace the user's normal library resume.
    if (_rt->playWavTestMode) {
      return false;
    }
    // v9.1.1: do not persist resume while the player is in an SD/read error
    // recovery state. Keep the previous known-good resume point.
    if (_rt->mediaState == MEDIA_STATE_ERROR || _rt->audioHealth == 0) {
      return false;
    }

    // v9.1.1 Next Track Engine:
    // Only the active decoder/RT path is allowed to advance persistent resume.
    // Playlist navigation (next/prev) may change mediaPath for UI selection,
    // but it must not overwrite the last real playback position.
    const bool activeDecoder = _rt->audioPlaying || _rt->rtAudioTaskRunning || _rt->mediaState == MEDIA_STATE_PLAYING;
    // v9.1.1: track-only resume still needs a trusted source. EOF/STOP/prev/next
    // are not trusted save sources because they can race with handoff. Save normal
    // auto-resume only while a decoder is actively playing, or on explicit rsave.
    if (!activeDecoder && !_rt->resumeAutoSaveRequest) {
      return false;
    }

    // Unsupported decoders must not become the saved resume target after
    // next/prev. Save resume only for supported live playback paths.
    if ((_rt->mediaCodec == MEDIA_CODEC_MP3 || _rt->mediaCodec == MEDIA_CODEC_OPUS ||
         _rt->mediaCodec == MEDIA_CODEC_OGG || _rt->mediaCodec == MEDIA_CODEC_AAC ||
         _rt->mediaCodec == MEDIA_CODEC_NONE) && !activeDecoder) {
      return false;
    }
    _rt->volumePercent = _audio->getVolume();
    _rt->queueCount = _queue->size();
    _rt->queueIndex = _queue->index();
    _rt->queueShuffleSmart = _queue->shuffleSmart();
    _rt->queueRepeatMode = _queue->repeatMode();
    String cur = _queue->current();
    if (cur.length()) copyPath(_rt->queueCurrent, sizeof(_rt->queueCurrent), cur.c_str());
    bool qok = _db->saveQueue();
    bool rok = _db->saveResume();
    if (rok) Serial.printf("[SMARTRESUME] autosave #%lu track-only path=%s volume=%d\n",
                           (unsigned long)_rt->resumeSaveCount, _rt->mediaPath,
                           _audio->getVolume());
    return qok || rok;
  }

  void print() const {
    if (!_rt) return;
    Serial.printf("[SMARTRESUME] enabled=%d mode=TRACK_ONLY restored=%d saves=%lu lastAuto=%lu path=%s playlist=%d/%d loadedVolume=%d\n",
                  _rt->resumeAutoEnabled ? 1 : 0, _rt->resumeRestored ? 1 : 0,
                  (unsigned long)_rt->resumeSaveCount, (unsigned long)_rt->resumeLastAutoSaveMs,
                  _rt->resumeLastPath,
                  _rt->resumeLoadedPlaylistIndex >= 0 ? _rt->resumeLoadedPlaylistIndex + 1 : 0,
                  _rt->resumeLoadedPlaylistCount, _rt->resumeLoadedVolume);
  }

private:
  RuntimeState* _rt = nullptr;
  DatabaseService* _db = nullptr;
  QueueManager* _queue = nullptr;
  AudioEngine* _audio = nullptr;
  bool _dirty = false;
  uint32_t _lastSaveMs = 0;
  const uint32_t _minSaveIntervalMs = 15000;
  char _lastPath[192] = "";
  uint8_t _lastProgress = 0;
  uint32_t _lastPlayed = 0;
  int _lastQueueIndex = -1;
  int _lastQueueCount = 0;
  bool _lastShuffle = false;
  int _lastRepeat = 2;
  int _lastVolume = 42;

  bool shouldHoldRestoredPosition() const {
    if (!_rt) return false;
    if (!_rt->resumeRestored) return false;
    if (_rt->resumeLoadedProgress == 0 && _rt->resumeLoadedPositionBytes == 0) return false;
    if (_rt->audioPlaying || _rt->rtAudioTaskRunning) return false;
    if (_rt->mediaCodec == MEDIA_CODEC_WAV) return false;
    if (strncmp(_rt->mediaPath, _rt->resumeLastPath, sizeof(_rt->resumeLastPath)) != 0) return false;
    if (_rt->mediaProgress != 0 || _rt->mediaPlayedBytes != 0) return false;
    return true;
  }

  bool pathChanged() const {
    if (!_rt) return false;
    return strncmp(_lastPath, _rt->mediaPath, sizeof(_lastPath)) != 0;
  }
  static void copyPath(char* dst, size_t n, const char* src) {
    if (!dst || n == 0) return;
    if (!src) src = "";
    strncpy(dst, src, n - 1);
    dst[n - 1] = 0;
  }
};
