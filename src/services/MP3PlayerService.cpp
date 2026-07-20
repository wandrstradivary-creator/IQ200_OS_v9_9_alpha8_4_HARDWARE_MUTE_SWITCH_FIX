#include "MP3PlayerService.h"
#include <Arduino.h>
#include <string.h>
#include <esp_heap_caps.h>
#define DR_MP3_IMPLEMENTATION
#include <dr_mp3.h>

namespace {
size_t mp3ReadProc(void* userData, void* bufferOut, size_t bytesToRead) {
  auto* stream = static_cast<SDManager::Stream*>(userData);
  return stream ? stream->read(bufferOut, bytesToRead, 3) : 0;
}

drmp3_bool32 mp3SeekProc(void* userData, int offset, drmp3_seek_origin origin) {
  auto* stream = static_cast<SDManager::Stream*>(userData);
  if (!stream) return DRMP3_FALSE;
  const SeekMode mode = (static_cast<int>(origin) == 1) ? SeekCur : SeekSet;
  return stream->seek(static_cast<int64_t>(offset), mode) ? DRMP3_TRUE : DRMP3_FALSE;
}

drmp3_bool32 mp3TellProc(void* userData, drmp3_int64* cursor) {
  auto* stream = static_cast<SDManager::Stream*>(userData);
  if (!stream || !cursor) return DRMP3_FALSE;
  *cursor = static_cast<drmp3_int64>(stream->position());
  return DRMP3_TRUE;
}
}

void MP3PlayerService::taskEntry(void* arg) {
  auto* self = static_cast<MP3PlayerService*>(arg);
  self->taskRunning = true;
  self->rtLoop();
  self->taskRunning = false;
  self->taskHandle = nullptr;
  vTaskDelete(nullptr);
}

void MP3PlayerService::closeDecoderOnly() {
  if (decoder) {
    drmp3_uninit(static_cast<drmp3*>(decoder));
    delete static_cast<drmp3*>(decoder);
    decoder = nullptr;
  }
  mp3Stream.close();
}

void MP3PlayerService::resetMeters() { vuL = vuR = 0; }

void MP3PlayerService::updateVU(const int16_t* pcmStereo, size_t frames) {
  if (!pcmStereo || !frames) return;
  int32_t peakL = 0, peakR = 0;
  for (size_t i = 0; i < frames; i += 4) {
    int32_t l = pcmStereo[i * 2];
    int32_t r = pcmStereo[i * 2 + 1];
    if (l < 0) l = -l;
    if (r < 0) r = -r;
    if (l > peakL) peakL = l;
    if (r > peakR) peakR = r;
  }
  int32_t nli = (peakL * 100L) / 32767L;
  int32_t nri = (peakR * 100L) / 32767L;
  if (nli > 100) nli = 100;
  if (nri > 100) nri = 100;
  uint8_t nl = (uint8_t)nli;
  uint8_t nr = (uint8_t)nri;
  vuL = (uint8_t)(((uint16_t)vuL * 3 + nl) / 4);
  vuR = (uint8_t)(((uint16_t)vuR * 3 + nr) / 4);
}

bool MP3PlayerService::begin() {
  const size_t bytes = framesPerRead * 2 * sizeof(int16_t);
  if (!pcmBuf) {
    pcmBuf = (int16_t*)heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!pcmBuf) pcmBuf = (int16_t*)ps_malloc(bytes);
    if (!pcmBuf) pcmBuf = (int16_t*)malloc(bytes);
  }
  if (!stereoBuf) {
    stereoBuf = (int16_t*)heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!stereoBuf) stereoBuf = (int16_t*)ps_malloc(bytes);
    if (!stereoBuf) stereoBuf = (int16_t*)malloc(bytes);
  }
  if (pcmBuf && stereoBuf) {
    Serial.printf("[MP3] buffers frames=%u bytesEach=%u ready=1\n", (unsigned)framesPerRead, (unsigned)bytes);
  }
  return pcmBuf && stereoBuf;
}

bool MP3PlayerService::open(const char* path, AudioEngine& audio) {
  stop();
  if (!begin()) return false;
  if (!mp3Stream.open(path, FILE_READ)) {
    Serial.printf("[MP3][SDM] open failed path=%s\n", path ? path : "");
    return false;
  }

  drmp3* mp3 = nullptr;
  // Some files/cards need a clean reopen after the first parser attempt.
  // Keep init bounded: never scan the complete MP3 during open().
  for (uint8_t attempt = 1; attempt <= 2; ++attempt) {
    mp3 = new drmp3();
    if (mp3 && drmp3_init(mp3, mp3ReadProc, mp3SeekProc, mp3TellProc,
                          nullptr, &mp3Stream, nullptr)) {
      break;
    }
    if (mp3) { delete mp3; mp3 = nullptr; }
    mp3Stream.close();
    if (attempt < 2) {
      Serial.printf("[MP3][INIT] retry=%u path=%s\n", (unsigned)attempt, path ? path : "");
      vTaskDelay(pdMS_TO_TICKS(20));
      taskYIELD();
      if (!mp3Stream.open(path, FILE_READ)) break;
    }
  }
  if (!mp3) {
    mp3Stream.close();
    Serial.printf("[MP3][SDM] decoder init failed path=%s\n", path ? path : "");
    return false;
  }
  if (mp3->channels < 1 || mp3->channels > 2 || mp3->sampleRate == 0) {
    drmp3_uninit(mp3); delete mp3; mp3Stream.close(); return false;
  }
  if (!audio.begin(mp3->sampleRate)) {
    drmp3_uninit(mp3); delete mp3; mp3Stream.close(); return false;
  }

  decoder = mp3;
  infoData.sampleRate = mp3->sampleRate;
  infoData.channels = mp3->channels;
  infoData.bitsPerSample = 16;
  // Do NOT call drmp3_get_pcm_frame_count() here: it scans the whole file,
  // blocks Core0 in SD reads and can trigger the task watchdog on large MP3s.
  infoData.totalPCMFrames = 0;
  compressedBytesTotal = mp3Stream.size();
  compressedBytesPlayed = mp3Stream.position();
  infoData.totalPCMBytes = compressedBytesTotal;
  audioRef = &audio;
  framesPlayed = 0;
  firstChunk = true;
  stopRequested = false;
  playing = true;
  readError = false;
  naturalEof = false;
  underruns = shortWrites = rtLoops = lastChunkBytes = stackHighWater = 0;
  sdRetries = sdErrors = 0;
  resetMeters();
  strncpy(lastErrorText, "READY", sizeof(lastErrorText)-1);
  lastErrorText[sizeof(lastErrorText)-1] = 0;
  audio.primeSilence(80);

  BaseType_t ok = xTaskCreatePinnedToCore(taskEntry, "audio_rt_mp3", 16384, this, 5, &taskHandle, 0);
  if (ok != pdPASS) {
    playing = false;
    closeDecoderOnly();
    taskHandle = nullptr;
    return false;
  }
  return true;
}

void MP3PlayerService::rtLoop() {
  Serial.printf("[AUDIO-RT] v9.7-alpha2 STREAMING MP3 task core=%d framesPerRead=%u\n",
                xPortGetCoreID(), (unsigned)framesPerRead);
  auto* mp3 = static_cast<drmp3*>(decoder);
  while (playing && !stopRequested && mp3) {
    rtLoops++;
    if ((rtLoops & 0x1F) == 0) stackHighWater = uxTaskGetStackHighWaterMark(nullptr);

    drmp3_uint64 frames = drmp3_read_pcm_frames_s16(mp3, framesPerRead, pcmBuf);
    if (frames == 0 && !stopRequested && mp3Stream.position() + 16U < mp3Stream.size()) {
      bool recovered = false;
      for (int attempt = 1; attempt <= 3 && !stopRequested; ++attempt) {
        vTaskDelay(pdMS_TO_TICKS(20 * attempt));
        taskYIELD();
        frames = drmp3_read_pcm_frames_s16(mp3, framesPerRead, pcmBuf);
        if (frames) {
          sdRetries++;
          recovered = true;
          Serial.printf("[AUDIO-RT][SDRETRY] MP3 recovered attempt=%d pos=%lu/%lu\n",
                        attempt, (unsigned long)mp3Stream.position(),
                        (unsigned long)mp3Stream.size());
          break;
        }
      }
      if (!recovered) {
        readError = true;
        sdErrors++;
        shortWrites++;
        snprintf(lastErrorText, sizeof(lastErrorText), "MP3 read stopped early %lu/%lu",
                 (unsigned long)mp3Stream.position(),
                 (unsigned long)mp3Stream.size());
        Serial.printf("[AUDIO-RT][SDERR] %s\n", lastErrorText);
        break;
      }
    }
    if (!frames) {
      naturalEof = !stopRequested;
      strncpy(lastErrorText, naturalEof ? "EOF" : "STOPPED", sizeof(lastErrorText)-1);
      lastErrorText[sizeof(lastErrorText)-1] = 0;
      break;
    }

    int16_t* out = pcmBuf;
    size_t outFrames = (size_t)frames;
    if (infoData.channels == 1) {
      for (size_t i = 0; i < outFrames; ++i) {
        int16_t s = pcmBuf[i];
        stereoBuf[i*2] = s;
        stereoBuf[i*2+1] = s;
      }
      out = stereoBuf;
    }
    const size_t pcmLen = outFrames * 4;
    lastChunkBytes = pcmLen;
    if (firstChunk) {
      audioRef->fadeInPCM16Stereo((uint8_t*)out, pcmLen, 1024);
      firstChunk = false;
    }
    updateVU(out, outFrames);
    size_t written = 0;
    if (!audioRef || !audioRef->writePCMAll((const uint8_t*)out, pcmLen, &written)) {
      shortWrites++;
      break;
    }
    if (written != pcmLen) shortWrites++;
    framesPlayed += frames;
    compressedBytesPlayed = mp3Stream.position();
    if (compressedBytesPlayed > compressedBytesTotal) compressedBytesPlayed = compressedBytesTotal;
    // Match FLAC fairness: taskYIELD() cannot schedule the lower-priority
    // Core0 WebServer worker. PCM is already queued in I2S DMA here.
    vTaskDelay(pdMS_TO_TICKS(1));
  }

  playing = false;
  stopRequested = false;
  if (naturalEof) compressedBytesPlayed = compressedBytesTotal;
  resetMeters();
  closeDecoderOnly();
  if (audioRef) audioRef->primeSilence(20);
  Serial.printf("[AUDIO-RT] MP3 task stopped state=%s underruns=%lu shortWrites=%lu loops=%lu\n",
    readError ? "SDERR" : (naturalEof ? "EOF" : "STOP"),
    (unsigned long)underruns, (unsigned long)shortWrites, (unsigned long)rtLoops);
}

void MP3PlayerService::stop() {
  if (taskHandle || playing || taskRunning) {
    stopRequested = true;
    playing = false;
    uint32_t start = millis();
    while (taskRunning && millis() - start < 900) vTaskDelay(pdMS_TO_TICKS(5));
  }
  if (taskRunning && taskHandle) {
    vTaskDelete(taskHandle);
    taskHandle = nullptr;
    taskRunning = false;
  }
  closeDecoderOnly();
  playing = false;
  if (stopRequested) {
    strncpy(lastErrorText, "STOPPED", sizeof(lastErrorText)-1);
    lastErrorText[sizeof(lastErrorText)-1] = 0;
  }
  stopRequested = false;
  firstChunk = true;
  resetMeters();
}
