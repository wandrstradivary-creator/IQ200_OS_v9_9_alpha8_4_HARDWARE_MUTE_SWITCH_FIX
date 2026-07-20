#pragma once
#include <Arduino.h>

class NotificationManager {
  char last[64] = "READY";
  uint32_t lastMs = 0;

public:
  void notify(const char* msg) {
    if (!msg) return;
    strncpy(last, msg, sizeof(last) - 1);
    last[sizeof(last) - 1] = 0;
    lastMs = millis();
  }

  const char* message() const { return last; }
  bool active(uint32_t timeoutMs = 3000) const { return millis() - lastMs < timeoutMs; }
};
