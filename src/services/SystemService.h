#pragma once
#include <Arduino.h>

class SystemService {
  uint32_t bootMs = 0;
public:
  void begin() { bootMs = millis(); }
  uint32_t uptimeSec() const { return (millis() - bootMs) / 1000; }
  uint32_t flashMB() const { return ESP.getFlashChipSize() / 1024 / 1024; }
  uint32_t heapFree() const { return ESP.getFreeHeap(); }
  uint32_t psramKB() const { return ESP.getPsramSize() / 1024; }
  uint32_t psramFreeKB() const { return ESP.getFreePsram() / 1024; }
};
