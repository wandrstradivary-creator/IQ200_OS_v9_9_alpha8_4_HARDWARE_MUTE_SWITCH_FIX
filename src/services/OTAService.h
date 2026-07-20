#pragma once
#include <Arduino.h>

class OTAService {
  bool enabled = false;
public:
  void begin() { enabled = false; }
  bool isEnabled() const { return enabled; }
  const char* status() const { return enabled ? "READY" : "DISABLED"; }
};
