#pragma once

#include <Arduino.h>
#include <Preferences.h>

enum IQ200Mode : uint8_t {
  IQ200_MODE_CENTER = 0,
  IQ200_MODE_LOCAL_PLAYER = 1,
  IQ200_MODE_WEBRADIO = 2,
  IQ200_MODE_BLUETOOTH = 3,
  IQ200_MODE_RADIO = 4
};

class ModeManager {
  static constexpr const char* NVS_NAMESPACE = "iq200mode";
  static constexpr uint8_t MAX_EARLY_BOOT_FAILURES = 3;

  IQ200Mode activeMode = IQ200_MODE_CENTER;
  uint8_t earlyBootFailures = 0;
  bool healthy = false;

  static bool validStoredMode(uint8_t value) {
    return value <= static_cast<uint8_t>(IQ200_MODE_RADIO);
  }

  bool writeState(IQ200Mode mode, uint8_t failures) {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false)) return false;
    const bool modeOk = prefs.putUChar("mode", static_cast<uint8_t>(mode)) == 1;
    const bool failuresOk = prefs.putUChar("fail", failures) == 1;
    prefs.end();
    return modeOk && failuresOk;
  }

public:
  IQ200Mode begin(bool forceCenter) {
    Preferences prefs;
    uint8_t stored = static_cast<uint8_t>(IQ200_MODE_CENTER);
    uint8_t failures = 0;
    if (prefs.begin(NVS_NAMESPACE, true)) {
      stored = prefs.getUChar("mode", static_cast<uint8_t>(IQ200_MODE_CENTER));
      failures = prefs.getUChar("fail", 0);
      prefs.end();
    }
    const bool storedModeUnavailable = !validStoredMode(stored) ||
                                       !available(static_cast<IQ200Mode>(stored));
    if (storedModeUnavailable) {
      stored = static_cast<uint8_t>(IQ200_MODE_CENTER);
    }

    activeMode = static_cast<IQ200Mode>(stored);
    if (forceCenter || (activeMode != IQ200_MODE_CENTER && failures >= MAX_EARLY_BOOT_FAILURES)) {
      Serial.printf("[MODE] recovery -> CENTER force=%d earlyFailures=%u\n",
                    forceCenter ? 1 : 0, static_cast<unsigned>(failures));
      activeMode = IQ200_MODE_CENTER;
      failures = 0;
      writeState(activeMode, failures);
    } else if (activeMode != IQ200_MODE_CENTER) {
      failures = failures < 255U ? static_cast<uint8_t>(failures + 1U) : 255U;
      writeState(activeMode, failures);
    } else if (failures != 0 || storedModeUnavailable) {
      failures = 0;
      writeState(activeMode, failures);
    }

    earlyBootFailures = failures;
    healthy = activeMode == IQ200_MODE_CENTER;
    Serial.printf("[MODE] boot mode=%s id=%u earlyFailures=%u\n",
                  name(activeMode), static_cast<unsigned>(activeMode),
                  static_cast<unsigned>(earlyBootFailures));
    return activeMode;
  }

  IQ200Mode current() const { return activeMode; }
  bool is(IQ200Mode mode) const { return activeMode == mode; }
  bool isHealthy() const { return healthy; }
  uint8_t failureCount() const { return earlyBootFailures; }

  bool markHealthy() {
    if (healthy) return true;
    healthy = writeState(activeMode, 0);
    if (healthy) {
      earlyBootFailures = 0;
      Serial.printf("[MODE] healthy mode=%s\n", name(activeMode));
    }
    return healthy;
  }

  bool setNext(IQ200Mode target) {
    if (!available(target)) return false;
    const bool ok = writeState(target, 0);
    Serial.printf("[MODE] next=%s save=%d\n", name(target), ok ? 1 : 0);
    return ok;
  }

  static bool available(IQ200Mode mode) {
    return mode == IQ200_MODE_CENTER || mode == IQ200_MODE_LOCAL_PLAYER || mode == IQ200_MODE_WEBRADIO;
  }

  static const char* name(IQ200Mode mode) {
    switch (mode) {
      case IQ200_MODE_CENTER: return "MODE_CENTER";
      case IQ200_MODE_LOCAL_PLAYER: return "LOCAL_PLAYER";
      case IQ200_MODE_WEBRADIO: return "WEBRADIO";
      case IQ200_MODE_BLUETOOTH: return "BLUETOOTH_FUTURE";
      case IQ200_MODE_RADIO: return "RADIO_FUTURE";
      default: return "UNKNOWN";
    }
  }

  static IQ200Mode parse(const String& requested) {
    String value = requested;
    value.trim();
    value.toLowerCase();
    if (value == "0" || value == "center" || value == "modecenter" || value == "mode_center") return IQ200_MODE_CENTER;
    if (value == "1" || value == "local" || value == "player" || value == "local_player") return IQ200_MODE_LOCAL_PLAYER;
    if (value == "2" || value == "webradio" || value == "web_radio") return IQ200_MODE_WEBRADIO;
    if (value == "3" || value == "bluetooth" || value == "bt") return IQ200_MODE_BLUETOOTH;
    if (value == "4" || value == "radio" || value == "fm") return IQ200_MODE_RADIO;
    return static_cast<IQ200Mode>(255);
  }
};
