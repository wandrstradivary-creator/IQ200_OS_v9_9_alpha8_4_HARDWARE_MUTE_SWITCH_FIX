#pragma once
#include <Arduino.h>
#include <SD.h>
#include "AudioEngine.h"
#include "WavParser.h"

// IQ200 OS v7.2 Media Foundation
// Dedicated WAV playback task pinned to Core0.
// Stabilized: counters, short-write guard, no Serial spam in RT loop.
// UI/CoreOS must not pump audio directly anymore.
class WavPlayerService {
  File file;
  WavInfo infoData;

  volatile bool playing = false;
  volatile bool stopRequested = false;
  volatile bool taskRunning = false;
  volatile uint32_t bytesPlayed = 0;
  volatile uint8_t vuL = 0;
  volatile uint8_t vuR = 0;
  volatile uint32_t underruns = 0;
  volatile uint32_t shortWrites = 0;
  volatile uint32_t rtLoops = 0;
  volatile uint32_t lastChunkBytes = 0;
  volatile uint32_t stackHighWater = 0;

  bool firstChunk = true;
  AudioEngine* audioRef = nullptr;
  TaskHandle_t taskHandle = nullptr;

  uint8_t* inBuf = nullptr;
  uint8_t* outBuf = nullptr;
  size_t inBufSize = 4096;   // SRAM-first. Good SD/I2S balance for ESP32-S3.
  size_t outBufSize = 8192;  // mono -> stereo expansion buffer.

  static void taskEntry(void* arg) {
    WavPlayerService* self = static_cast<WavPlayerService*>(arg);
    self->taskRunning = true;
    self->rtLoop();
    self->taskRunning = false;
    self->taskHandle = nullptr;
    vTaskDelete(nullptr);
  }

  void closeFileOnly() {
    if (file) file.close();
  }

  void resetMeters() {
    vuL = 0;
    vuR = 0;
  }

  void updateVU(const uint8_t* pcm, size_t len) {
    if (!pcm || len < 4) return;
    len &= ~((size_t)3);
    const int16_t* s = (const int16_t*)pcm;
    size_t frames = len / 4;
    int32_t peakL = 0;
    int32_t peakR = 0;

    // Decimate for lower CPU cost. Enough for a visual meter.
    for (size_t i = 0; i < frames; i += 4) {
      int32_t l = s[i * 2];
      int32_t r = s[i * 2 + 1];
      if (l < 0) l = -l;
      if (r < 0) r = -r;
      if (l > peakL) peakL = l;
      if (r > peakR) peakR = r;
    }

    uint8_t nl = (uint8_t)((peakL * 100L) / 32767L);
    uint8_t nr = (uint8_t)((peakR * 100L) / 32767L);
    if (nl > 100) nl = 100;
    if (nr > 100) nr = 100;

    // Smooth inside RT path; UI only reads two bytes.
    vuL = (uint8_t)(((uint16_t)vuL * 3 + nl) / 4);
    vuR = (uint8_t)(((uint16_t)vuR * 3 + nr) / 4);
  }

  void rtLoop() {
    Serial.printf("[AUDIO-RT] v9.1.1 task running on core %d\n", xPortGetCoreID());

    while (playing && !stopRequested) {
      rtLoops++;
      if ((rtLoops & 0x1F) == 0) stackHighWater = uxTaskGetStackHighWaterMark(nullptr);
      uint32_t played = bytesPlayed;
      uint32_t left = infoData.dataSize > played ? infoData.dataSize - played : 0;
      if (left == 0) break;

      size_t toRead = left > inBufSize ? inBufSize : left;
      if (infoData.channels == 2) toRead &= ~((size_t)3);
      else toRead &= ~((size_t)1);
      if (toRead == 0) break;

      size_t n = file.read(inBuf, toRead);
      if (n == 0) {
        underruns++;
        break;
      }
      lastChunkBytes = n;

      const uint8_t* pcm = inBuf;
      size_t pcmLen = n;

      if (infoData.channels == 1) {
        size_t samples = n / 2;
        if (samples * 4 > outBufSize) samples = outBufSize / 4;

        int16_t* src = (int16_t*)inBuf;
        int16_t* dst = (int16_t*)outBuf;

        for (size_t i = 0; i < samples; i++) {
          int16_t smp = src[i];
          dst[i * 2] = smp;
          dst[i * 2 + 1] = smp;
        }

        pcm = outBuf;
        pcmLen = samples * 4;
      } else {
        pcmLen &= ~((size_t)3);
      }

      if (firstChunk) {
        audioRef->fadeInPCM16Stereo((uint8_t*)pcm, pcmLen, 1024);
        firstChunk = false;
      }

      updateVU(pcm, pcmLen);

      size_t written = 0;
      if (!audioRef || !audioRef->writePCMAll(pcm, pcmLen, &written)) {
        shortWrites++;
        break;
      }
      if (written != pcmLen) shortWrites++;

      bytesPlayed = played + n;

      // taskYIELD() does not wake the lower-priority WebServer worker. PCM is
      // already queued in I2S DMA, so one blocked tick keeps Web online while
      // preserving the audio pipeline.
      vTaskDelay(pdMS_TO_TICKS(1));
    }

    playing = false;
    stopRequested = false;
    bytesPlayed = (bytesPlayed > infoData.dataSize) ? infoData.dataSize : bytesPlayed;
    resetMeters();
    closeFileOnly();
    if (audioRef) audioRef->primeSilence(20);
    Serial.printf("[AUDIO-RT] task stopped underruns=%lu shortWrites=%lu loops=%lu\n",
      (unsigned long)underruns, (unsigned long)shortWrites, (unsigned long)rtLoops);
  }

public:
  bool begin() {
    if (!inBuf) {
      inBuf = (uint8_t*)malloc(inBufSize);
      if (!inBuf) inBuf = (uint8_t*)ps_malloc(inBufSize);
    }
    if (!outBuf) {
      outBuf = (uint8_t*)malloc(outBufSize);
      if (!outBuf) outBuf = (uint8_t*)ps_malloc(outBufSize);
    }
    return inBuf != nullptr && outBuf != nullptr;
  }

  bool open(const char* path, AudioEngine& audio) {
    stop();
    if (!begin()) return false;

    file = SD.open(path, FILE_READ);
    if (!file) return false;

    if (!WavParser::parse(file, infoData)) {
      file.close();
      return false;
    }

    if (infoData.bitsPerSample != 16 || (infoData.channels != 1 && infoData.channels != 2)) {
      file.close();
      return false;
    }

    if (!audio.begin(infoData.sampleRate)) {
      file.close();
      return false;
    }

    file.seek(infoData.dataOffset);
    audioRef = &audio;
    bytesPlayed = 0;
    firstChunk = true;
    stopRequested = false;
    resetMeters();
    underruns = 0;
    shortWrites = 0;
    rtLoops = 0;
    lastChunkBytes = 0;
    stackHighWater = 0;

    audio.primeSilence(80);
    playing = true;

    BaseType_t ok = xTaskCreatePinnedToCore(
      taskEntry,
      "audio_rt_wav",
      12288,
      this,
      5,
      &taskHandle,
      0
    );

    if (ok != pdPASS) {
      playing = false;
      file.close();
      taskHandle = nullptr;
      return false;
    }

    return true;
  }

  void stop() {
    if (taskHandle || playing || taskRunning) {
      stopRequested = true;
      playing = false;
      uint32_t start = millis();
      while (taskRunning && millis() - start < 700) {
        vTaskDelay(pdMS_TO_TICKS(5));
      }
    }

    if (taskRunning && taskHandle) {
      vTaskDelete(taskHandle);
      taskHandle = nullptr;
      taskRunning = false;
    }

    closeFileOnly();
    playing = false;
    stopRequested = false;
    bytesPlayed = 0;
    firstChunk = true;
    resetMeters();
  }

  bool isPlaying() const { return playing; }
  bool isTaskRunning() const { return taskRunning; }
  uint32_t played() const { return bytesPlayed; }
  uint32_t total() const { return infoData.dataSize; }
  uint8_t vuLeft() const { return vuL; }
  uint8_t vuRight() const { return vuR; }
  uint32_t underrunCount() const { return underruns; }
  uint32_t shortWriteCount() const { return shortWrites; }
  uint32_t loopCount() const { return rtLoops; }
  uint32_t lastChunk() const { return lastChunkBytes; }
  uint32_t taskStackHighWater() const { return stackHighWater; }
  uint8_t health() const {
    uint32_t bad = underruns + shortWrites;
    if (bad >= 100) return 0;
    return (uint8_t)(100 - bad);
  }
  const WavInfo& info() const { return infoData; }

  int progressPercent() const {
    if (!infoData.dataSize) return 0;
    uint32_t p = bytesPlayed;
    if (p > infoData.dataSize) p = infoData.dataSize;
    return (int)((uint64_t)p * 100ULL / infoData.dataSize);
  }

  // Compatibility: CoreOS should not use this in v7.1.
  bool pump(AudioEngine&, uint32_t = 0) { return isPlaying(); }
};
