#pragma once

#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// IQ200 OS v9.3-alpha2
// Central SD arbitration, stream transport and diagnostics foundation.
class SDManager {
public:
  struct Stats {
    uint32_t lockCount = 0;
    uint32_t lockTimeouts = 0;
    uint32_t crossCoreAccess = 0;
    uint32_t openCount = 0;
    uint32_t openFailures = 0;
    uint32_t readCount = 0;
    uint32_t readRetries = 0;
    uint32_t readFailures = 0;
    uint64_t readBytes = 0;
    uint32_t seekCount = 0;
    uint32_t closeCount = 0;
    uint32_t existsCount = 0;
    uint32_t mkdirCount = 0;
    uint32_t removeCount = 0;
    uint32_t renameCount = 0;
    uint32_t backgroundDenied = 0;
    uint32_t streamSessions = 0;
    uint32_t recoveries = 0;
    uint32_t recoveryFailures = 0;
    uint32_t recoveryAttempts = 0;
    uint32_t clockDownshifts = 0;
    uint32_t currentFrequency = 0;
    uint32_t lastWaitUs = 0;
    uint32_t maxWaitUs = 0;
    int8_t lastCore = -1;
  };

  class Guard {
  public:
    explicit Guard(TickType_t timeout = portMAX_DELAY)
      : locked_(SDManager::lock(timeout)) {}
    ~Guard() { if (locked_) SDManager::unlock(); }

    Guard(const Guard&) = delete;
    Guard& operator=(const Guard&) = delete;

    bool locked() const { return locked_; }
    explicit operator bool() const { return locked_; }

  private:
    bool locked_;
  };

  // Safe long-lived file transport. Every operation is serialized through the
  // SDManager mutex, so decoder callbacks never touch File/SD directly.
  class Stream {
  public:
    Stream() = default;
    ~Stream() { close(); }

    Stream(const Stream&) = delete;
    Stream& operator=(const Stream&) = delete;

    bool open(const char* path, const char* mode = FILE_READ);
    size_t read(void* buffer, size_t bytes, uint8_t retries = 3);
    bool seek(int64_t offset, SeekMode mode = SeekSet);
    uint32_t position();
    uint32_t size();
    int available();
    bool valid();
    void close();
    const char* path() const { return path_.c_str(); }

  private:
    bool refillCache();
    bool reopenAt(uint32_t position, bool recoveryBridge);
    void invalidateCache();

    File file_;
    String path_;
    uint8_t* cache_ = nullptr;
    size_t cacheCapacity_ = 0;
    size_t cacheLength_ = 0;
    size_t cacheOffset_ = 0;
    uint32_t cacheStart_ = 0;
    uint32_t logicalPos_ = 0;
    uint32_t fileSize_ = 0;
  };

  static bool begin(uint8_t csPin, SPIClass& spi, uint32_t frequency = 12000000U);
  static bool initialized();
  static bool mounted();
  static bool streamActive();
  static bool recovering();
  static bool backgroundAllowed();

  static bool lock(TickType_t timeout = portMAX_DELAY);
  static void unlock();

  // Immediate-call helpers. For streaming sessions use SDManager::Stream.
  static File open(const char* path, const char* mode = FILE_READ);
  static bool exists(const char* path);
  static bool mkdir(const char* path);
  static bool remove(const char* path);
  static bool rename(const char* from, const char* to);
  static uint8_t cardType();
  static uint64_t cardSize();

  static Stats stats();
  static void resetStats();
  static void printStats(::Stream& out = Serial);
  static uint32_t currentFrequency();
  static uint8_t governorLevel();
  static bool setFrequency(uint32_t frequency);
  static bool supportedFrequency(uint32_t frequency);

private:
  static bool ensureMutex();
  static void noteAccess();
  static void noteOpen(bool ok);
  static void noteRead(size_t bytes, bool retry, bool failure);
  static void noteSeek();
  static void noteClose();
  static void noteBackgroundDenied();
  static void beginStreamSession();
  static void endStreamSession();
  static bool remountAt(uint32_t frequency);
  static bool recoveryBridgeTo12M();
  static bool noteRecovery(bool ok);

  static SemaphoreHandle_t mutex_;
  static portMUX_TYPE statsMux_;
  static Stats stats_;
  static bool initialized_;
  static bool mounted_;
  static int8_t ownerCore_;
  static uint32_t activeStreams_;
  static uint8_t csPin_;
  static SPIClass* spi_;
  static uint32_t frequency_;
  static volatile bool recovering_;
};
