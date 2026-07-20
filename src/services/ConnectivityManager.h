#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include <DNSServer.h>
#include "RuntimeState.h"

class ConnectivityManager {
  RuntimeState* rt = nullptr;
  Preferences prefs;
  String savedSsid;
  String savedPassword;
  String hostName = "iq200";
  uint32_t connectStartedMs = 0;
  uint32_t lastReconnectMs = 0;
  uint32_t reconnectCount = 0;
  bool staWanted = false;
  bool apWanted = false;
  bool mdnsStarted = false;
  DNSServer dnsServer;
  bool captiveDnsStarted = false;
  bool autoConnectEnabled = true;
  bool fallbackApEnabled = true;
  bool fallbackStarted = false;
  static constexpr uint32_t CONNECT_TIMEOUT_MS = 15000;
  static constexpr uint32_t RECONNECT_INTERVAL_MS = 10000;

  void syncPolicyRuntime() {
    if (!rt) return;
    rt->wifiAutoConnect = autoConnectEnabled;
    rt->wifiFallbackAp = fallbackApEnabled;
    rt->wifiReconnectCount = reconnectCount;
  }

  void updateRuntime() {
    if (!rt) return;
    const bool staOk = WiFi.status() == WL_CONNECTED;
    const wifi_mode_t mode = WiFi.getMode();
    const bool apOk = (mode == WIFI_AP || mode == WIFI_AP_STA) && WiFi.softAPIP() != IPAddress(0, 0, 0, 0);
    rt->wifiConnected = staOk || apOk;
    rt->wifiApMode = apOk;
    rt->wifiStaMode = staWanted;
    rt->wifiRssi = staOk ? WiFi.RSSI() : 0;
    String ip = staOk ? WiFi.localIP().toString() : (apOk ? WiFi.softAPIP().toString() : String("0.0.0.0"));
    strncpy(rt->wifiIp, ip.c_str(), sizeof(rt->wifiIp) - 1);
    rt->wifiIp[sizeof(rt->wifiIp) - 1] = 0;
    strncpy(rt->wifiSsid, staOk ? WiFi.SSID().c_str() : (apOk ? "IQ200-OS" : ""), sizeof(rt->wifiSsid) - 1);
    rt->wifiSsid[sizeof(rt->wifiSsid) - 1] = 0;
    syncPolicyRuntime();
  }

  void startMdnsIfReady() {
    if (mdnsStarted || WiFi.status() != WL_CONNECTED) return;
    if (MDNS.begin(hostName.c_str())) {
      MDNS.addService("http", "tcp", 80);
      mdnsStarted = true;
      Serial.printf("[NET] mDNS ready: http://%s.local/\n", hostName.c_str());
    } else Serial.println("[NET] mDNS start failed");
  }

  void startCaptiveDnsIfReady() {
    const wifi_mode_t mode = WiFi.getMode();
    const bool apOk = (mode == WIFI_AP || mode == WIFI_AP_STA) && WiFi.softAPIP() != IPAddress(0,0,0,0);
    if (apOk && !captiveDnsStarted) {
      dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
      captiveDnsStarted = dnsServer.start(53, "*", WiFi.softAPIP());
      Serial.printf("[NET][PORTAL] DNS redirect %s ip=%s\n", captiveDnsStarted ? "started" : "failed", WiFi.softAPIP().toString().c_str());
    } else if (!apOk && captiveDnsStarted) {
      dnsServer.stop();
      captiveDnsStarted = false;
      Serial.println("[NET][PORTAL] DNS redirect stopped");
    }
  }

  void savePolicy() {
    prefs.begin("iq200wifi", false);
    prefs.putBool("auto", autoConnectEnabled);
    prefs.putBool("fallback", fallbackApEnabled);
    prefs.end();
    syncPolicyRuntime();
  }

public:
  void begin(RuntimeState& state) {
    rt = &state;
    prefs.begin("iq200wifi", true);
    savedSsid = prefs.getString("ssid", "");
    savedPassword = prefs.getString("pass", "");
    hostName = prefs.getString("host", "iq200");
    autoConnectEnabled = prefs.getBool("auto", true);
    fallbackApEnabled = prefs.getBool("fallback", true);
    prefs.end();
    rt->connectivityReady = true;
    strncpy(rt->wifiIp, "0.0.0.0", sizeof(rt->wifiIp) - 1);
    rt->wifiIp[sizeof(rt->wifiIp) - 1] = 0;
    strncpy(rt->wifiSavedSsid, savedSsid.c_str(), sizeof(rt->wifiSavedSsid) - 1);
    rt->wifiSavedSsid[sizeof(rt->wifiSavedSsid) - 1] = 0;
    syncPolicyRuntime();
    Serial.printf("[NET][BOOT] foundation ready saved=%d auto=%d fallbackAP=%d host=%s.local\n",
      savedSsid.length() ? 1 : 0, autoConnectEnabled ? 1 : 0, fallbackApEnabled ? 1 : 0, hostName.c_str());
  }

  void boot() {
    fallbackStarted = false;
    if (autoConnectEnabled && savedSsid.length()) {
      Serial.printf("[NET][BOOT] autoconnect ssid='%s' timeout=%lums\n", savedSsid.c_str(), (unsigned long)CONNECT_TIMEOUT_MS);
      startSta(savedSsid.c_str(), savedPassword.c_str(), false, false);
    } else if (fallbackApEnabled) {
      Serial.printf("[NET][BOOT] %s; starting fallback AP\n", savedSsid.length() ? "autoconnect disabled" : "no saved profile");
      startAp("IQ200-OS");
      fallbackStarted = true;
    } else {
      Serial.println("[NET][BOOT] WiFi remains OFF");
      if (rt) strncpy(rt->wifiStatus, "BOOT_OFF", sizeof(rt->wifiStatus) - 1);
    }
    if (rt) rt->webEnableRequest = true;
  }

  bool saveCredentials(const String& ssid, const String& password) {
    prefs.begin("iq200wifi", false);
    const bool ok1 = prefs.putString("ssid", ssid) > 0;
    const bool ok2 = prefs.putString("pass", password) >= 0;
    prefs.end();
    if (ok1 && ok2) {
      savedSsid = ssid; savedPassword = password;
      if (rt) {
        strncpy(rt->wifiSavedSsid, savedSsid.c_str(), sizeof(rt->wifiSavedSsid) - 1);
        rt->wifiSavedSsid[sizeof(rt->wifiSavedSsid) - 1] = 0;
        rt->wifiProfileSaved = true;
      }
    }
    return ok1 && ok2;
  }

  bool forgetCredentials() {
    prefs.begin("iq200wifi", false);
    const bool a = prefs.remove("ssid");
    prefs.remove("pass");
    prefs.end();
    savedSsid = ""; savedPassword = "";
    if (rt) {
      rt->wifiSavedSsid[0] = 0;
      rt->wifiProfileSaved = false;
    }
    Serial.printf("[NET] saved profile forgotten result=%d\n", a ? 1 : 0);
    return true;
  }

  void setAutoConnect(bool enabled) { autoConnectEnabled = enabled; savePolicy(); Serial.printf("[NET][CFG] autoconnect=%d saved=1\n", enabled ? 1 : 0); }
  void setFallbackAp(bool enabled) { fallbackApEnabled = enabled; savePolicy(); Serial.printf("[NET][CFG] fallbackAP=%d saved=1\n", enabled ? 1 : 0); }
  bool autoConnect() const { return autoConnectEnabled; }
  bool fallbackAp() const { return fallbackApEnabled; }

  bool loadAndConnect(bool keepAp = false) {
    if (!savedSsid.length()) { Serial.println("[NET] no saved WiFi profile"); return false; }
    return startSta(savedSsid.c_str(), savedPassword.c_str(), keepAp, false);
  }

  bool startSta(const char* ssid, const char* password, bool keepAp = false, bool save = true) {
    if (!ssid || !ssid[0]) return false;
    apWanted = keepAp;
    staWanted = true;
    fallbackStarted = keepAp;
    mdnsStarted = false;
    WiFi.mode(keepAp ? WIFI_AP_STA : WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.persistent(false);
    WiFi.begin(ssid, password ? password : "");
    connectStartedMs = millis();
    lastReconnectMs = connectStartedMs;
    if (save) saveCredentials(ssid, password ? password : "");
    if (rt) {
      strncpy(rt->wifiStatus, "CONNECTING", sizeof(rt->wifiStatus) - 1);
      rt->wifiStatus[sizeof(rt->wifiStatus) - 1] = 0;
    }
    Serial.printf("[NET] STA connecting ssid='%s' mode=%s\n", ssid, keepAp ? "AP+STA" : "STA");
    return true;
  }

  void startAp(const char* ssid = "IQ200-OS", const char* password = "") {
    apWanted = true;
    WiFi.mode(staWanted ? WIFI_AP_STA : WIFI_AP);
    bool ok = (password && strlen(password) >= 8) ? WiFi.softAP(ssid, password) : WiFi.softAP(ssid);
    if (rt) {
      strncpy(rt->wifiStatus, ok ? (staWanted ? "AP_STA_CONNECTING" : "AP_READY") : "AP_FAIL", sizeof(rt->wifiStatus) - 1);
      rt->wifiStatus[sizeof(rt->wifiStatus) - 1] = 0;
    }
    updateRuntime();
    Serial.printf("[NET] AP %s ssid='%s' ip=%s\n", ok ? "OK" : "FAIL", ssid, WiFi.softAPIP().toString().c_str());
    if (ok) startCaptiveDnsIfReady();
  }

  void startApSta(const char* ssid, const char* password) { startAp("IQ200-OS"); startSta(ssid, password, true, true); }

  int scan() {
    const wifi_mode_t previousMode = WiFi.getMode();
    WiFi.mode(apWanted ? WIFI_AP_STA : WIFI_STA);
    int count = WiFi.scanNetworks(false, true);
    Serial.printf("[NET] scan found=%d\n", count);
    for (int i = 0; i < count; ++i) {
      Serial.printf("  %2d  RSSI=%4d  CH=%2d  %s%s\n", i + 1, WiFi.RSSI(i), WiFi.channel(i), WiFi.SSID(i).c_str(), WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? " [open]" : "");
      delay(0);
    }
    WiFi.scanDelete();
    if (!staWanted && !apWanted) WiFi.mode(previousMode);
    if (rt) rt->wifiNetworks = count;
    return count;
  }

  void disconnectSta(bool eraseSaved = false) {
    WiFi.disconnect(false, eraseSaved);
    staWanted = false; mdnsStarted = false;
    if (eraseSaved) forgetCredentials();
    if (apWanted) WiFi.mode(WIFI_AP); else WiFi.mode(WIFI_OFF);
    startCaptiveDnsIfReady();
    if (rt) strncpy(rt->wifiStatus, apWanted ? "AP_READY" : "OFF", sizeof(rt->wifiStatus) - 1);
    updateRuntime();
    Serial.println("[NET] STA disconnected");
  }

  void stop() {
    if (mdnsStarted) { MDNS.end(); mdnsStarted = false; }
    if (captiveDnsStarted) { dnsServer.stop(); captiveDnsStarted = false; }
    WiFi.disconnect(true); WiFi.softAPdisconnect(true); WiFi.mode(WIFI_OFF);
    staWanted = false; apWanted = false; fallbackStarted = false;
    if (rt) {
      rt->wifiConnected = false; rt->wifiApMode = false; rt->wifiStaMode = false;
      strncpy(rt->wifiIp, "0.0.0.0", sizeof(rt->wifiIp)-1);
      strncpy(rt->wifiStatus, "OFF", sizeof(rt->wifiStatus)-1);
    }
    Serial.println("[NET] WiFi off");
  }

  void tick() {
    if (!rt) return;
    rt->networkTicks++;
    const wl_status_t st = WiFi.status();
    const uint32_t now = millis();
    if (staWanted && st == WL_CONNECTED) {
      strncpy(rt->wifiStatus, apWanted ? "AP_STA_READY" : "STA_READY", sizeof(rt->wifiStatus)-1);
      startMdnsIfReady();
    } else if (staWanted) {
      if (!fallbackStarted && fallbackApEnabled && now - connectStartedMs >= CONNECT_TIMEOUT_MS) {
        Serial.printf("[NET][BOOT] STA timeout status=%d; enabling fallback AP while reconnect continues\n", (int)st);
        startAp("IQ200-OS");
        fallbackStarted = true;
        if (rt) rt->webEnableRequest = true;
      }
      if (now - lastReconnectMs >= RECONNECT_INTERVAL_MS) {
        lastReconnectMs = now;
        reconnectCount++;
        Serial.printf("[NET] STA reconnect #%lu ssid='%s' status=%d\n", (unsigned long)reconnectCount, savedSsid.c_str(), (int)st);
        WiFi.reconnect();
      }
      strncpy(rt->wifiStatus, fallbackStarted ? "AP_STA_RETRY" : "CONNECTING", sizeof(rt->wifiStatus)-1);
    }
    rt->wifiStatus[sizeof(rt->wifiStatus)-1] = 0;
    startCaptiveDnsIfReady();
    if (captiveDnsStarted) dnsServer.processNextRequest();
    updateRuntime();
  }

  void print() const {
    Serial.printf("[NET] ready=%d status=%s connected=%d sta=%d ap=%d ssid='%s' saved='%s' ip=%s rssi=%d auto=%d fallback=%d reconnects=%lu host=%s.local portal=%d ticks=%lu\n",
      rt&&rt->connectivityReady, rt?rt->wifiStatus:"?", rt&&rt->wifiConnected, rt&&rt->wifiStaMode, rt&&rt->wifiApMode,
      rt?rt->wifiSsid:"", rt?rt->wifiSavedSsid:"", rt?rt->wifiIp:"", rt?rt->wifiRssi:0,
      autoConnectEnabled?1:0, fallbackApEnabled?1:0, (unsigned long)reconnectCount,
      hostName.c_str(), captiveDnsStarted ? 1 : 0, rt?(unsigned long)rt->networkTicks:0);
  }
};
