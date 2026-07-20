#pragma once
#include <Arduino.h>
#include "RuntimeState.h"

// v9.1.1 Next Track Engine: small non-blocking UX/presentation supervisor.
// Keeps UI labels, theme/status metadata and polish counters in one place.
class CommercialPolishService {
  RuntimeState* rt = nullptr;
  uint32_t lastTickMs = 0;
public:
  void begin(RuntimeState& state) {
    rt = &state;
    rt->commercialPolishEnabled = true;
    rt->commercialThemeId = 1;
    rt->commercialIconSetId = 1;
    rt->commercialHelpClean = true;
    rt->commercialPolishTicks = 0;
    strncpy(rt->commercialThemeName, "Enterprise Dark", sizeof(rt->commercialThemeName)-1);
    rt->commercialThemeName[sizeof(rt->commercialThemeName)-1] = 0;
    strncpy(rt->commercialUiStatus, "POLISHED", sizeof(rt->commercialUiStatus)-1);
    rt->commercialUiStatus[sizeof(rt->commercialUiStatus)-1] = 0;
  }

  void tick() {
    if (!rt || !rt->commercialPolishEnabled) return;
    uint32_t now = millis();
    if (now - lastTickMs < 1000) return;
    lastTickMs = now;
    rt->commercialPolishTicks++;
  }

  void print() const {
    if (!rt) return;
    Serial.println("[POLISH] ===== IQ200 v9.1.1 Next Track Engine =====");
    Serial.printf("[POLISH] enabled=%d theme=%s themeId=%u icons=%u cleanHelp=%d ticks=%lu\n",
      rt->commercialPolishEnabled ? 1 : 0,
      rt->commercialThemeName,
      (unsigned)rt->commercialThemeId,
      (unsigned)rt->commercialIconSetId,
      rt->commercialHelpClean ? 1 : 0,
      (unsigned long)rt->commercialPolishTicks);
    Serial.printf("[POLISH] UI=%s full=%lu partial=%lu FPS frames=%lu\n",
      rt->commercialUiStatus,
      (unsigned long)rt->fullFrames,
      (unsigned long)rt->partialFrames,
      (unsigned long)rt->rendererFrames);
  }
};
