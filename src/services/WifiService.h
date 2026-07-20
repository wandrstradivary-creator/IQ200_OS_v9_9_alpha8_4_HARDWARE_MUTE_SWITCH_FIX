#pragma once
#include <Arduino.h>
#include <WiFi.h>

class WifiService {
public:
  int lastCount = -1;

  int scan() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);
    delay(150);
    lastCount = WiFi.scanNetworks(false, true);
    WiFi.mode(WIFI_OFF);
    return lastCount;
  }

  String ssid(int i) { return WiFi.SSID(i); }
  int rssi(int i) { return WiFi.RSSI(i); }
};
