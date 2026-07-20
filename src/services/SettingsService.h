#pragma once
#include <Arduino.h>
#include <Preferences.h>

class SettingsService {
  Preferences prefs;
  int volume = 8;

public:
  void begin() {
    prefs.begin("iq200", false);
    volume = prefs.getInt("volume", 8);
  }

  void setVolume(int v) {
    if (v < 0) v = 0;
    if (v > 100) v = 100;
    volume = v;
    prefs.putInt("volume", volume);
  }

  int getVolume() const { return volume; }
};
