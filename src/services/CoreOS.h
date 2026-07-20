#pragma once
#include <Arduino.h>

enum CoreOSState : uint8_t {
  OS_BOOTING = 0,
  OS_READY,
  OS_ERROR
};

class CoreOS {
  CoreOSState state = OS_BOOTING;
  uint32_t bootTime = 0;
  char version[16] = "6.0";

public:
  void begin() {
    bootTime = millis();
    state = OS_READY;
  }

  CoreOSState getState() const { return state; }
  const char* getVersion() const { return version; }
  uint32_t uptimeSec() const { return (millis() - bootTime) / 1000; }

  const char* stateName() const {
    switch (state) {
      case OS_BOOTING: return "BOOTING";
      case OS_READY: return "READY";
      case OS_ERROR: return "ERROR";
      default: return "UNKNOWN";
    }
  }
};
