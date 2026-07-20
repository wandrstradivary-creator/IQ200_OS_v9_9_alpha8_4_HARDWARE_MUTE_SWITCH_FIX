#pragma once
#include <Arduino.h>

class WatchdogService {
  uint32_t lastCore0Beat = 0;
  uint32_t lastCore1Beat = 0;

public:
  void begin() {
    lastCore0Beat = millis();
    lastCore1Beat = millis();
  }

  void beatCore0() { lastCore0Beat = millis(); }
  void beatCore1() { lastCore1Beat = millis(); }

  uint32_t core0AgeMs() const { return millis() - lastCore0Beat; }
  uint32_t core1AgeMs() const { return millis() - lastCore1Beat; }

  bool core0Ok(uint32_t limitMs = 3000) const { return core0AgeMs() < limitMs; }
  bool core1Ok(uint32_t limitMs = 3000) const { return core1AgeMs() < limitMs; }
};
