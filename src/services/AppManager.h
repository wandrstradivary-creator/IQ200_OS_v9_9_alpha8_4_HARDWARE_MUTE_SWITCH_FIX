#pragma once
#include <Arduino.h>

enum IQAppId : uint8_t {
  APP_STATUS = 0,
  APP_DISPLAY,
  APP_PSRAM,
  APP_ENCODERS,
  APP_AUDIO,
  APP_WIFI,
  APP_SD,
  APP_FILES,
  APP_PLAYER,
  APP_SETTINGS,
  APP_TASKS,
  APP_HEALTH,
  APP_SCHEDULER
};

class AppManager {
  IQAppId current = APP_STATUS;
  IQAppId previous = APP_STATUS;
  uint32_t switches = 0;

public:
  void open(IQAppId app) {
    previous = current;
    current = app;
    switches++;
  }

  IQAppId currentApp() const { return current; }
  IQAppId previousApp() const { return previous; }
  uint32_t switchCount() const { return switches; }

  const char* name(IQAppId app) const {
    switch (app) {
      case APP_STATUS: return "Status";
      case APP_DISPLAY: return "Display";
      case APP_PSRAM: return "PSRAM";
      case APP_ENCODERS: return "Encoders";
      case APP_AUDIO: return "Audio";
      case APP_WIFI: return "WiFi";
      case APP_SD: return "SD";
      case APP_FILES: return "Files";
      case APP_PLAYER: return "Player";
      case APP_SETTINGS: return "Settings";
      case APP_TASKS: return "Tasks";
      case APP_HEALTH: return "Health";
      case APP_SCHEDULER: return "Scheduler";
      default: return "Unknown";
    }
  }
};
