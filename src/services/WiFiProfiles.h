#pragma once
#include <Arduino.h>
#include <Preferences.h>

class WiFiProfiles {
  Preferences prefs;
public:
  void begin() { prefs.begin("wifi", false); }

  void save(const String& ssid, const String& pass) {
    prefs.putString("ssid", ssid);
    prefs.putString("pass", pass);
  }

  String ssid() { return prefs.getString("ssid", ""); }
  String pass() { return prefs.getString("pass", ""); }
  bool hasProfile() { return ssid().length() > 0; }
};
