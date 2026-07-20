#include "SDManager.h"

#include "esp_timer.h"
#include <esp_heap_caps.h>

SemaphoreHandle_t SDManager::mutex_ = nullptr;
portMUX_TYPE SDManager::statsMux_ = portMUX_INITIALIZER_UNLOCKED;
SDManager::Stats SDManager::stats_{};
bool SDManager::initialized_ = false;
bool SDManager::mounted_ = false;
int8_t SDManager::ownerCore_ = 0;
uint32_t SDManager::activeStreams_ = 0;
uint8_t SDManager::csPin_ = 0;
SPIClass* SDManager::spi_ = nullptr;
uint32_t SDManager::frequency_ = 12000000U;
volatile bool SDManager::recovering_ = false;

bool SDManager::ensureMutex() {
  if (mutex_) return true;

  SemaphoreHandle_t candidate = xSemaphoreCreateRecursiveMutex();
  if (!candidate) {
    Serial.println("[SDM] recursive mutex allocation failed");
    return false;
  }

  taskENTER_CRITICAL(&statsMux_);
  if (!mutex_) {
    mutex_ = candidate;
    candidate = nullptr;
  }
  taskEXIT_CRITICAL(&statsMux_);

  if (candidate) vSemaphoreDelete(candidate);
  return mutex_ != nullptr;
}

void SDManager::noteAccess() {
  const int8_t core = static_cast<int8_t>(xPortGetCoreID());
  taskENTER_CRITICAL(&statsMux_);
  stats_.lastCore = core;
  if (initialized_ && core != ownerCore_) stats_.crossCoreAccess++;
  taskEXIT_CRITICAL(&statsMux_);
}

void SDManager::noteOpen(bool ok) {
  taskENTER_CRITICAL(&statsMux_);
  stats_.openCount++;
  if (!ok) stats_.openFailures++;
  taskEXIT_CRITICAL(&statsMux_);
}

void SDManager::noteRead(size_t bytes, bool retry, bool failure) {
  taskENTER_CRITICAL(&statsMux_);
  stats_.readCount++;
  stats_.readBytes += bytes;
  if (retry) stats_.readRetries++;
  if (failure) stats_.readFailures++;
  taskEXIT_CRITICAL(&statsMux_);
}

void SDManager::noteSeek() {
  taskENTER_CRITICAL(&statsMux_);
  stats_.seekCount++;
  taskEXIT_CRITICAL(&statsMux_);
}

void SDManager::noteClose() {
  taskENTER_CRITICAL(&statsMux_);
  stats_.closeCount++;
  taskEXIT_CRITICAL(&statsMux_);
}

bool SDManager::begin(uint8_t csPin, SPIClass& spi, uint32_t frequency) {
  if (!ensureMutex()) return false;
  Guard guard;
  if (!guard) return false;

  initialized_ = true;
  ownerCore_ = static_cast<int8_t>(xPortGetCoreID());
  noteAccess();

  csPin_ = csPin;
  spi_ = &spi;
  frequency_ = frequency;
  taskENTER_CRITICAL(&statsMux_);
  stats_.currentFrequency = frequency_;
  taskEXIT_CRITICAL(&statsMux_);
  mounted_ = SD.begin(csPin, spi, frequency);
  Serial.printf("[SDM] begin core=%d task=%s freq=%lu ok=%d\n",
                xPortGetCoreID(), pcTaskGetName(nullptr),
                static_cast<unsigned long>(frequency), mounted_ ? 1 : 0);
  return mounted_;
}

bool SDManager::initialized() { return initialized_; }
bool SDManager::mounted() { return mounted_; }

void SDManager::noteBackgroundDenied() {
  taskENTER_CRITICAL(&statsMux_);
  stats_.backgroundDenied++;
  taskEXIT_CRITICAL(&statsMux_);
}

void SDManager::beginStreamSession() {
  taskENTER_CRITICAL(&statsMux_);
  activeStreams_++;
  stats_.streamSessions++;
  taskEXIT_CRITICAL(&statsMux_);
}

void SDManager::endStreamSession() {
  taskENTER_CRITICAL(&statsMux_);
  if (activeStreams_ > 0) activeStreams_--;
  taskEXIT_CRITICAL(&statsMux_);
}

bool SDManager::streamActive() {
  taskENTER_CRITICAL(&statsMux_);
  const bool active = activeStreams_ > 0;
  taskEXIT_CRITICAL(&statsMux_);
  return active;
}

bool SDManager::recovering() { return recovering_; }

bool SDManager::backgroundAllowed() {
  return !recovering_ && !streamActive();
}

bool SDManager::lock(TickType_t timeout) {
  if (!ensureMutex()) return false;

  const int64_t started = esp_timer_get_time();
  const BaseType_t ok = xSemaphoreTakeRecursive(mutex_, timeout);
  const uint32_t waited = static_cast<uint32_t>(esp_timer_get_time() - started);

  taskENTER_CRITICAL(&statsMux_);
  stats_.lastWaitUs = waited;
  if (waited > stats_.maxWaitUs) stats_.maxWaitUs = waited;
  if (ok == pdTRUE) stats_.lockCount++;
  else stats_.lockTimeouts++;
  taskEXIT_CRITICAL(&statsMux_);

  if (ok == pdTRUE) noteAccess();
  return ok == pdTRUE;
}

void SDManager::unlock() {
  if (mutex_) xSemaphoreGiveRecursive(mutex_);
}

File SDManager::open(const char* path, const char* mode) {
  if (!backgroundAllowed()) { noteBackgroundDenied(); return File(); }
  Guard guard;
  if (!guard) return File();
  File file = SD.open(path, mode);
  noteOpen(static_cast<bool>(file));
  return file;
}

bool SDManager::exists(const char* path) {
  if (!backgroundAllowed()) { noteBackgroundDenied(); return false; }
  Guard guard;
  if (!guard) return false;
  taskENTER_CRITICAL(&statsMux_);
  stats_.existsCount++;
  taskEXIT_CRITICAL(&statsMux_);
  return SD.exists(path);
}

bool SDManager::mkdir(const char* path) {
  if (!backgroundAllowed()) { noteBackgroundDenied(); return false; }
  Guard guard;
  if (!guard) return false;
  taskENTER_CRITICAL(&statsMux_);
  stats_.mkdirCount++;
  taskEXIT_CRITICAL(&statsMux_);
  return SD.mkdir(path);
}

bool SDManager::remove(const char* path) {
  if (!backgroundAllowed()) { noteBackgroundDenied(); return false; }
  Guard guard;
  if (!guard) return false;
  taskENTER_CRITICAL(&statsMux_);
  stats_.removeCount++;
  taskEXIT_CRITICAL(&statsMux_);
  return SD.remove(path);
}

bool SDManager::rename(const char* from, const char* to) {
  if (!backgroundAllowed()) { noteBackgroundDenied(); return false; }
  Guard guard;
  if (!guard) return false;
  taskENTER_CRITICAL(&statsMux_);
  stats_.renameCount++;
  taskEXIT_CRITICAL(&statsMux_);
  return SD.rename(from, to);
}

uint8_t SDManager::cardType() {
  if (!backgroundAllowed()) { noteBackgroundDenied(); return CARD_NONE; }
  Guard guard;
  return guard ? SD.cardType() : CARD_NONE;
}

uint64_t SDManager::cardSize() {
  if (!backgroundAllowed()) { noteBackgroundDenied(); return 0; }
  Guard guard;
  return guard ? SD.cardSize() : 0;
}

bool SDManager::Stream::open(const char* path, const char* mode) {
  close();
  if (!path || !path[0]) return false;

  if (!cache_) {
    cacheCapacity_ = 128U * 1024U;
    cache_ = static_cast<uint8_t*>(heap_caps_malloc(cacheCapacity_, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!cache_) cache_ = static_cast<uint8_t*>(ps_malloc(cacheCapacity_));
    if (!cache_) {
      cacheCapacity_ = 32U * 1024U;
      cache_ = static_cast<uint8_t*>(malloc(cacheCapacity_));
    }
  }

  Guard guard;
  if (!guard) return false;
  file_ = SD.open(path, mode);
  path_ = path;
  const bool ok = static_cast<bool>(file_);
  SDManager::noteOpen(ok);
  if (ok) {
    fileSize_ = file_.size();
    logicalPos_ = file_.position();
    invalidateCache();
    SDManager::beginStreamSession();
    Serial.printf("[SDM][PREFETCH] cache=%uKB physicalChunk=8KB path=%s\n",
                  static_cast<unsigned>(cacheCapacity_ / 1024U), path_.c_str());
  }
  return ok;
}

void SDManager::Stream::invalidateCache() {
  cacheLength_ = 0;
  cacheOffset_ = 0;
  cacheStart_ = logicalPos_;
}

bool SDManager::noteRecovery(bool ok) {
  taskENTER_CRITICAL(&statsMux_);
  if (ok) stats_.recoveries++;
  else stats_.recoveryFailures++;
  taskEXIT_CRITICAL(&statsMux_);
  return ok;
}

bool SDManager::remountAt(uint32_t frequency) {
  if (!spi_) return noteRecovery(false);
  taskENTER_CRITICAL(&statsMux_);
  stats_.recoveryAttempts++;
  taskEXIT_CRITICAL(&statsMux_);

  const uint32_t previous = frequency_;
  SD.end();
  vTaskDelay(pdMS_TO_TICKS(50));
  frequency_ = frequency;
  mounted_ = SD.begin(csPin_, *spi_, frequency_);
  taskENTER_CRITICAL(&statsMux_);
  stats_.currentFrequency = frequency_;
  if (frequency_ < previous) stats_.clockDownshifts++;
  taskEXIT_CRITICAL(&statsMux_);
  Serial.printf("[SDM][RECOVERY] remount %lu -> %lu Hz ok=%d\n",
                static_cast<unsigned long>(previous),
                static_cast<unsigned long>(frequency_), mounted_ ? 1 : 0);
  return noteRecovery(mounted_);
}

bool SDManager::recoveryBridgeTo12M() {
  // v9.8-alpha13: block every background SD client for the complete remount
  // window. No Resume/Artwork/DB File object may be created while VFS is reset.
  recovering_ = true;
  Serial.println("[SDM][BRIDGE] enter 8 MHz recovery bridge (background locked)");
  bool ok = remountAt(8000000U);
  if (ok) {
    vTaskDelay(pdMS_TO_TICKS(20));
    Serial.println("[SDM][BRIDGE] card responded at 8 MHz; restoring 12 MHz");
    ok = remountAt(12000000U);
  }
  if (ok) Serial.println("[SDM][BRIDGE] working clock restored to 12 MHz");
  recovering_ = false;
  return ok;
}

bool SDManager::Stream::reopenAt(uint32_t position, bool recoveryBridge) {
  if (path_.isEmpty()) return false;
  if (file_) file_.close();
  invalidateCache();

  if (recoveryBridge) {
    if (!SDManager::recoveryBridgeTo12M()) return false;
  } else if (!SDManager::remountAt(SDManager::currentFrequency())) {
    return false;
  }

  file_ = SD.open(path_.c_str(), FILE_READ);
  SDManager::noteOpen(static_cast<bool>(file_));
  if (!file_) return false;
  fileSize_ = file_.size();
  if (position > fileSize_) position = fileSize_;
  if (!file_.seek(position, SeekSet)) return false;
  logicalPos_ = position;
  invalidateCache();
  Serial.printf("[SDM][RECOVERY] stream reopened pos=%lu/%lu path=%s freq=%lu\n",
                static_cast<unsigned long>(logicalPos_),
                static_cast<unsigned long>(fileSize_), path_.c_str(),
                static_cast<unsigned long>(SDManager::currentFrequency()));
  return true;
}

bool SDManager::Stream::refillCache() {
  if (!cache_ || cacheCapacity_ == 0 || logicalPos_ >= fileSize_) return false;

  constexpr size_t kPhysicalChunk = 8U * 1024U;
  const uint32_t wantedPos = logicalPos_;
  invalidateCache();
  cacheStart_ = wantedPos;

  // v9.8-alpha11: normal playback always runs at 12 MHz. A transport error
  // uses 8 MHz only as a short remount bridge, then returns to 12 MHz before
  // reopen/seek and before any bytes are exposed to the decoder.
  for (uint8_t recoveryPass = 0; recoveryPass < 2; ++recoveryPass) {
    bool transportFailed = false;
    {
      Guard guard(pdMS_TO_TICKS(500));
      if (!guard) {
        transportFailed = true;
      } else if (!file_) {
        Serial.printf("[SDM][READ] invalid stream level=%u pos=%lu freq=%lu\n",
                      static_cast<unsigned>(recoveryPass),
                      static_cast<unsigned long>(wantedPos),
                      static_cast<unsigned long>(SDManager::currentFrequency()));
        transportFailed = true;
      } else {
        if (file_.position() != wantedPos && !file_.seek(wantedPos, SeekSet)) {
          Serial.printf("[SDM][READ] seek failed level=%u pos=%lu freq=%lu\n",
                        static_cast<unsigned>(recoveryPass),
                        static_cast<unsigned long>(wantedPos),
                        static_cast<unsigned long>(SDManager::currentFrequency()));
          transportFailed = true;
        }

        while (!transportFailed && cacheLength_ < cacheCapacity_ &&
               cacheStart_ + cacheLength_ < fileSize_) {
          const size_t remainFile = static_cast<size_t>(fileSize_ - (cacheStart_ + cacheLength_));
          const size_t ask = min(kPhysicalChunk, min(cacheCapacity_ - cacheLength_, remainFile));
          size_t got = 0;
          static constexpr uint16_t kRetryDelayMs[3] = {5, 15, 40};
          for (uint8_t attempt = 0; attempt < 3 && got == 0; ++attempt) {
            got = file_.read(cache_ + cacheLength_, ask);
            SDManager::noteRead(got, attempt > 0, false);
            if (got == 0) {
              Serial.printf("[SDM][READ] retry=%u level=%u pos=%lu ask=%u freq=%lu\n",
                            static_cast<unsigned>(attempt + 1U),
                            static_cast<unsigned>(recoveryPass),
                            static_cast<unsigned long>(wantedPos + cacheLength_),
                            static_cast<unsigned>(ask),
                            static_cast<unsigned long>(SDManager::currentFrequency()));
              if (attempt < 2) vTaskDelay(pdMS_TO_TICKS(kRetryDelayMs[attempt]));
            }
          }
          if (got == 0) {
            transportFailed = true;
            break;
          }
          cacheLength_ += got;
          if (got < ask) {
            // A short physical read before logical EOF is suspicious. Keep the
            // bytes already received, but force recovery before asking again.
            if (cacheStart_ + cacheLength_ < fileSize_) transportFailed = true;
            break;
          }
        }
      }
    } // release SD mutex before remount/reopen

    if (!transportFailed && cacheLength_ > 0) return true;

    // Do not expose partially filled data from a failed transport. dr_flac may
    // otherwise consume it and interpret the next zero callback as permanent EOF.
    cacheLength_ = 0;
    cacheOffset_ = 0;

    const uint32_t before = SDManager::currentFrequency();
    Serial.printf("[SDM][RECOVERY] begin level=%u pos=%lu freq=%lu bridge=8M->12M\n",
                  static_cast<unsigned>(recoveryPass),
                  static_cast<unsigned long>(wantedPos),
                  static_cast<unsigned long>(before));
    if (!reopenAt(wantedPos, true)) {
      Serial.printf("[SDM][RECOVERY] reopen failed level=%u pos=%lu freq=%lu\n",
                    static_cast<unsigned>(recoveryPass),
                    static_cast<unsigned long>(wantedPos),
                    static_cast<unsigned long>(SDManager::currentFrequency()));
      continue;
    }
    Serial.printf("[SDM][RECOVERY] resumed transport pos=%lu freq=%lu (was=%lu)\n",
                  static_cast<unsigned long>(wantedPos),
                  static_cast<unsigned long>(SDManager::currentFrequency()),
                  static_cast<unsigned long>(before));
  }

  SDManager::noteRead(0, true, true);
  return false;
}

size_t SDManager::Stream::read(void* buffer, size_t bytes, uint8_t retries) {
  (void)retries; // Recovery policy is handled by the prefetch/refill engine.
  if (!buffer || bytes == 0) return 0;
  if (logicalPos_ >= fileSize_) return 0;

  uint8_t* out = static_cast<uint8_t*>(buffer);
  size_t total = 0;
  while (total < bytes && logicalPos_ < fileSize_) {
    if (cacheOffset_ >= cacheLength_) {
      if (!refillCache()) break;
    }
    const size_t available = cacheLength_ - cacheOffset_;
    const size_t take = min(bytes - total, available);
    memcpy(out + total, cache_ + cacheOffset_, take);
    cacheOffset_ += take;
    logicalPos_ += take;
    total += take;
  }
  return total;
}

bool SDManager::Stream::seek(int64_t offset, SeekMode mode) {
  int64_t base = 0;
  if (mode == SeekCur) base = logicalPos_;
  else if (mode == SeekEnd) base = fileSize_;
  int64_t target = (mode == SeekSet) ? offset : base + offset;
  if (target < 0 || static_cast<uint64_t>(target) > fileSize_) return false;

  const uint32_t t = static_cast<uint32_t>(target);
  if (cacheLength_ > 0 && t >= cacheStart_ && t <= cacheStart_ + cacheLength_) {
    cacheOffset_ = t - cacheStart_;
    logicalPos_ = t;
    SDManager::noteSeek();
    return true;
  }

  Guard guard(pdMS_TO_TICKS(500));
  if (!guard || !file_) return false;
  const bool ok = file_.seek(t, SeekSet);
  if (ok) {
    logicalPos_ = t;
    invalidateCache();
  }
  SDManager::noteSeek();
  return ok;
}

uint32_t SDManager::Stream::position() { return logicalPos_; }
uint32_t SDManager::Stream::size() { return fileSize_; }
int SDManager::Stream::available() { return logicalPos_ < fileSize_ ? static_cast<int>(fileSize_ - logicalPos_) : 0; }

bool SDManager::Stream::valid() {
  Guard guard(pdMS_TO_TICKS(250));
  return guard && static_cast<bool>(file_);
}

void SDManager::Stream::close() {
  bool wasOpen = false;
  {
    Guard guard(pdMS_TO_TICKS(500));
    if (guard && file_) {
      wasOpen = true;
      file_.close();
      SDManager::noteClose();
    }
  }
  if (wasOpen) SDManager::endStreamSession();
  if (cache_) {
    free(cache_);
    cache_ = nullptr;
  }
  cacheCapacity_ = cacheLength_ = cacheOffset_ = 0;
  cacheStart_ = logicalPos_ = fileSize_ = 0;
  path_ = "";
}

bool SDManager::supportedFrequency(uint32_t frequency) {
  return frequency == 10000000U || frequency == 12000000U || frequency == 16000000U;
}

bool SDManager::setFrequency(uint32_t frequency) {
  if (!supportedFrequency(frequency) || !initialized_ || !spi_) return false;
  if (streamActive() || recovering_) {
    Serial.printf("[SDM][CLOCK] rejected active=%d recovering=%d requested=%lu\n",
                  streamActive() ? 1 : 0, recovering_ ? 1 : 0,
                  static_cast<unsigned long>(frequency));
    return false;
  }
  Guard guard(pdMS_TO_TICKS(1500));
  if (!guard) return false;
  recovering_ = true;
  const bool ok = remountAt(frequency);
  recovering_ = false;
  Serial.printf("[SDM][CLOCK] apply requested=%lu actual=%lu ok=%d\n",
                static_cast<unsigned long>(frequency),
                static_cast<unsigned long>(currentFrequency()), ok ? 1 : 0);
  return ok;
}

uint32_t SDManager::currentFrequency() {
  taskENTER_CRITICAL(&statsMux_);
  const uint32_t value = frequency_;
  taskEXIT_CRITICAL(&statsMux_);
  return value;
}

uint8_t SDManager::governorLevel() {
  const uint32_t hz = currentFrequency();
  if (hz <= 8000000U) return 2;
  if (hz <= 12000000U) return 1;
  return 0;
}

SDManager::Stats SDManager::stats() {
  taskENTER_CRITICAL(&statsMux_);
  Stats copy = stats_;
  taskEXIT_CRITICAL(&statsMux_);
  return copy;
}

void SDManager::resetStats() {
  taskENTER_CRITICAL(&statsMux_);
  stats_ = Stats{};
  taskEXIT_CRITICAL(&statsMux_);
}

void SDManager::printStats(::Stream& out) {
  const Stats s = stats();
  out.printf("[SDM] locks=%lu timeouts=%lu crossCore=%lu open=%lu fail=%lu "
             "read=%lu bytes=%llu retry=%lu rfail=%lu seek=%lu close=%lu "
             "exists=%lu mkdir=%lu remove=%lu rename=%lu denied=%lu sessions=%lu recover=%lu fatal=%lu "
             "recTry=%lu down=%lu freq=%lu level=%u wait=%luus max=%luus lastCore=%d\n",
             static_cast<unsigned long>(s.lockCount),
             static_cast<unsigned long>(s.lockTimeouts),
             static_cast<unsigned long>(s.crossCoreAccess),
             static_cast<unsigned long>(s.openCount),
             static_cast<unsigned long>(s.openFailures),
             static_cast<unsigned long>(s.readCount),
             static_cast<unsigned long long>(s.readBytes),
             static_cast<unsigned long>(s.readRetries),
             static_cast<unsigned long>(s.readFailures),
             static_cast<unsigned long>(s.seekCount),
             static_cast<unsigned long>(s.closeCount),
             static_cast<unsigned long>(s.existsCount),
             static_cast<unsigned long>(s.mkdirCount),
             static_cast<unsigned long>(s.removeCount),
             static_cast<unsigned long>(s.renameCount),
             static_cast<unsigned long>(s.backgroundDenied),
             static_cast<unsigned long>(s.streamSessions),
             static_cast<unsigned long>(s.recoveries),
             static_cast<unsigned long>(s.recoveryFailures),
             static_cast<unsigned long>(s.recoveryAttempts),
             static_cast<unsigned long>(s.clockDownshifts),
             static_cast<unsigned long>(currentFrequency()),
             static_cast<unsigned>(governorLevel()),
             static_cast<unsigned long>(s.lastWaitUs),
             static_cast<unsigned long>(s.maxWaitUs),
             static_cast<int>(s.lastCore));
}
