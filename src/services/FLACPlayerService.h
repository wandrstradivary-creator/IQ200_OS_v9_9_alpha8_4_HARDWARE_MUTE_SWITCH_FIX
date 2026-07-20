#pragma once
#include <Arduino.h>
#include "AudioEngine.h"
#include "SDManager.h"

// IQ200 OS v9.1.1 Next Track Engine Integration
// Real local FLAC path using dr_flac: SD/FATFS file -> FLAC decoder -> PCM16 stereo -> I2S -> PCM5102.
// dr_flac is supplied by the dr_libs dependency in platformio.ini.
class FLACPlayerService {
public:
  struct TransportContext {
    SDManager::Stream* stream = nullptr;
    uint32_t baseOffset = 0;
    uint32_t readCalls = 0;
    uint32_t seekCalls = 0;
    uint32_t tellCalls = 0;
    uint64_t bytesRead = 0;
    bool callbackError = false;
  };
  struct Info {
    uint32_t sampleRate = 0;
    uint16_t channels = 0;
    uint8_t bitsPerSample = 16;
    uint64_t totalPCMFrames = 0;
    uint64_t totalPCMBytes = 0;
  };

private:
  void* decoder = nullptr; // actually drflac*, hidden to keep this header light for Arduino builds
  SDManager::Stream flacStream;
  TransportContext transportCtx;
  Info infoData;
  volatile bool playing = false;
  volatile bool stopRequested = false;
  volatile bool taskRunning = false;
  volatile uint64_t framesPlayed = 0;
  volatile uint32_t bytesPlayedApprox = 0;
  volatile uint8_t vuL = 0;
  volatile uint8_t vuR = 0;
  volatile uint32_t underruns = 0;
  volatile uint32_t shortWrites = 0;
  volatile uint32_t rtLoops = 0;
  volatile uint32_t lastChunkBytes = 0;
  volatile uint32_t stackHighWater = 0;
  volatile uint32_t cacheHits = 0;
  volatile uint32_t cacheMisses = 0;
  volatile uint32_t readAheadBytes = 0;
  volatile uint32_t sdRetries = 0;
  volatile uint32_t sdErrors = 0;
  volatile uint8_t ringFillPct = 0;
  volatile uint8_t decoderLoadPct = 0;
  volatile bool readError = false;
  volatile bool naturalEof = false;
  char lastErrorText[72] = "READY";

  bool firstChunk = true;
  AudioEngine* audioRef = nullptr;
  TaskHandle_t taskHandle = nullptr;
  int16_t* pcmBuf = nullptr;
  int16_t* stereoBuf = nullptr;
  // v9.3-alpha2.7: decoder works in 4096-frame blocks. Compressed input is
  // prefetched separately into a 128 KB PSRAM cache by SDManager::Stream.
  size_t framesPerRead = 4096;

  static void taskEntry(void* arg);
  void rtLoop();
  void closeDecoderOnly();
  void resetMeters();
  void updateVU(const int16_t* pcmStereo, size_t frames);
  static void makeFatfsPath(const char* in, char* out, size_t outLen);

public:
  bool begin();
  bool open(const char* path, AudioEngine& audio);
  void stop();

  bool isPlaying() const { return playing; }
  bool isTaskRunning() const { return taskRunning; }
  uint32_t played() const { return bytesPlayedApprox; }
  uint32_t total() const { return infoData.totalPCMBytes > 0xFFFFFFFFULL ? 0xFFFFFFFFUL : (uint32_t)infoData.totalPCMBytes; }
  uint8_t progressPercent() const {
    if (infoData.totalPCMFrames == 0) return 0;
    uint64_t p = (framesPlayed * 100ULL) / infoData.totalPCMFrames;
    return (uint8_t)(p > 100 ? 100 : p);
  }
  uint8_t vuLeft() const { return vuL; }
  uint8_t vuRight() const { return vuR; }
  uint32_t underrunCount() const { return underruns; }
  uint32_t shortWriteCount() const { return shortWrites; }
  uint32_t loopCount() const { return rtLoops; }
  uint32_t lastChunk() const { return lastChunkBytes; }
  uint32_t taskStackHighWater() const { return stackHighWater; }
  uint32_t cacheHitCount() const { return cacheHits; }
  uint32_t cacheMissCount() const { return cacheMisses; }
  uint32_t readAhead() const { return readAheadBytes; }
  uint32_t retryCount() const { return sdRetries; }
  uint32_t sdErrorCount() const { return sdErrors; }
  uint8_t ringFill() const { return ringFillPct; }
  uint8_t decoderLoad() const { return decoderLoadPct; }
  bool failed() const { return readError; }
  bool finishedNaturally() const { return naturalEof; }
  const char* lastError() const { return lastErrorText; }
  uint8_t health() const {
    uint32_t faults = underruns + shortWrites;
    if (faults == 0) return 100;
    if (faults > 20) return 0;
    return (uint8_t)(100 - faults * 5);
  }
  const Info& info() const { return infoData; }
};
