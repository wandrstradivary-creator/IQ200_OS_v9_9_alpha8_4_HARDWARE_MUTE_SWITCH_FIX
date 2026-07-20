#pragma once
#include <Arduino.h>
#include "AudioEngine.h"
#include "SDManager.h"

// IQ200 OS v9.7-alpha1 MP3 Engine.
// SDManager::Stream -> dr_mp3 -> PCM16 stereo -> AudioEngine/I2S.
class MP3PlayerService {
public:
  struct Info {
    uint32_t sampleRate = 0;
    uint16_t channels = 0;
    uint8_t bitsPerSample = 16;
    uint64_t totalPCMFrames = 0; // unknown in streaming mode
    uint64_t totalPCMBytes = 0;  // compressed file bytes for progress
  };

private:
  void* decoder = nullptr; // drmp3*
  SDManager::Stream mp3Stream;
  Info infoData;
  AudioEngine* audioRef = nullptr;
  TaskHandle_t taskHandle = nullptr;
  int16_t* pcmBuf = nullptr;
  int16_t* stereoBuf = nullptr;
  size_t framesPerRead = 2304;
  bool firstChunk = true;

  volatile bool playing = false;
  volatile bool stopRequested = false;
  volatile bool taskRunning = false;
  volatile bool readError = false;
  volatile bool naturalEof = false;
  volatile uint64_t framesPlayed = 0;
  volatile uint32_t compressedBytesPlayed = 0;
  volatile uint32_t compressedBytesTotal = 0;
  volatile uint8_t vuL = 0;
  volatile uint8_t vuR = 0;
  volatile uint32_t underruns = 0;
  volatile uint32_t shortWrites = 0;
  volatile uint32_t rtLoops = 0;
  volatile uint32_t lastChunkBytes = 0;
  volatile uint32_t stackHighWater = 0;
  volatile uint32_t sdRetries = 0;
  volatile uint32_t sdErrors = 0;
  char lastErrorText[72] = "READY";

  static void taskEntry(void* arg);
  void rtLoop();
  void closeDecoderOnly();
  void resetMeters();
  void updateVU(const int16_t* pcmStereo, size_t frames);

public:
  bool begin();
  bool open(const char* path, AudioEngine& audio);
  void stop();

  bool isPlaying() const { return playing; }
  bool isTaskRunning() const { return taskRunning; }
  uint32_t played() const { return compressedBytesPlayed; }
  uint32_t total() const { return compressedBytesTotal; }
  uint8_t progressPercent() const {
    if (!compressedBytesTotal) return 0;
    uint64_t p = ((uint64_t)compressedBytesPlayed * 100ULL) / compressedBytesTotal;
    return (uint8_t)(p > 100 ? 100 : p);
  }
  uint8_t vuLeft() const { return vuL; }
  uint8_t vuRight() const { return vuR; }
  uint32_t underrunCount() const { return underruns; }
  uint32_t shortWriteCount() const { return shortWrites; }
  uint32_t loopCount() const { return rtLoops; }
  uint32_t lastChunk() const { return lastChunkBytes; }
  uint32_t taskStackHighWater() const { return stackHighWater; }
  uint32_t retryCount() const { return sdRetries; }
  uint32_t sdErrorCount() const { return sdErrors; }
  uint8_t health() const {
    uint32_t faults = underruns + shortWrites;
    if (!faults) return 100;
    if (faults > 20) return 0;
    return (uint8_t)(100 - faults * 5);
  }
  bool failed() const { return readError; }
  bool finishedNaturally() const { return naturalEof; }
  const char* lastError() const { return lastErrorText; }
  const Info& info() const { return infoData; }
};
