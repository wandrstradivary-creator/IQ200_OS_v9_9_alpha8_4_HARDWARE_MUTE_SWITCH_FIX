#pragma once
#include "SDManager.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <SD.h>
#include "RuntimeState.h"
#include "StorageService.h"
#include "CommandManager.h"
#include "PlaylistManager.h"
#include "ArtworkCache.h"
#include "RadioStationStore.h"
#include "RadioService.h"
#include "ModeManager.h"

class WebServerService {
  static constexpr size_t WEB_ARTWORK_MAX_BYTES = 256U * 1024U;
  static constexpr uint32_t WEB_ARTWORK_SEND_BUDGET_MS = 900U;
  RuntimeState* rt = nullptr;
  StorageService* storage = nullptr;
  CommandManager* commands = nullptr;
  PlaylistManager* playlist = nullptr;
  WebServer server{80};
  volatile bool running = false;
  uint32_t lastTickMs = 0;
  uint32_t gapWindowStartedMs = 0;
  uint32_t requests = 0;
  bool wifiScanActive = false;
  RadioStationStore& radioStations;
  RadioService* radioPlayback = nullptr;

  // Small cross-core command queue. WebServer runs on Core0; the proven
  // command parser consumes on Core1. No command handler is duplicated here.
  static constexpr uint8_t COMMAND_QUEUE_SIZE = 8;
  static constexpr size_t COMMAND_MAX_LEN = 191;
  char commandQueue[COMMAND_QUEUE_SIZE][COMMAND_MAX_LEN + 1]{};
  volatile uint8_t commandHead = 0;
  volatile uint8_t commandTail = 0;
  volatile uint8_t commandCount = 0;
  uint32_t commandAccepted = 0;
  uint32_t commandRejected = 0;
  char lastQueued[COMMAND_MAX_LEN + 1] = "";
  char lastExecuted[COMMAND_MAX_LEN + 1] = "";
  portMUX_TYPE commandMux = portMUX_INITIALIZER_UNLOCKED;

  String jsonEscape(const char* s) const {
    String out;
    if (!s) return out;
    while (*s) {
      char c = *s++;
      if (c == '"' || c == '\\') { out += '\\'; out += c; }
      else if (c == '\n') out += "\\n";
      else if (c == '\r') out += "\\r";
      else if (c == '\t') out += "\\t";
      else if ((uint8_t)c < 32) out += ' ';
      else out += c;
    }
    return out;
  }

  String statusJson() const {
    String j = "{";
    j.reserve(2048);
    RadioService::Snapshot radioSnapshot;
    if (radioPlayback) radioPlayback->snapshot(radioSnapshot);
    const uint32_t played = rt ? rt->mediaPlayedBytes : 0;
    const uint32_t total = rt ? rt->mediaDataSize : 0;
    const uint32_t seconds = (rt && rt->mediaSampleRate && rt->mediaChannels && rt->mediaBits)
      ? played / (rt->mediaSampleRate * rt->mediaChannels * (rt->mediaBits / 8U)) : 0;
    const uint32_t duration = (rt && rt->mediaSampleRate && rt->mediaChannels && rt->mediaBits)
      ? total / (rt->mediaSampleRate * rt->mediaChannels * (rt->mediaBits / 8U)) : 0;
    j += "\"version\":\"9.9-alpha7.13_WEBRADIO_WEB_PLAYER\",";
    j += "\"systemMode\":\""; j += jsonEscape(rt ? rt->systemModeName : "UNKNOWN"); j += "\",";
    j += "\"systemModeId\":"; j += String(rt ? rt->systemMode : 0); j += ",";
    j += "\"modeBootHealthy\":"; j += (rt && rt->modeBootHealthy ? "true" : "false"); j += ",";
    j += "\"modeEarlyBootFailures\":"; j += String(rt ? rt->modeEarlyBootFailures : 0); j += ",";
    j += "\"localPlayerAvailable\":"; j += (rt && rt->systemMode == IQ200_MODE_LOCAL_PLAYER ? "true" : "false"); j += ",";
    j += "\"webRadioAvailable\":"; j += (rt && rt->systemMode == IQ200_MODE_WEBRADIO ? "true" : "false"); j += ",";
    j += "\"sd\":"; j += (rt && rt->sdOk ? "true" : "false"); j += ",";
    j += "\"sdClockMHz\":"; j += String(SDManager::currentFrequency() / 1000000U); j += ",";
    j += "\"tracks\":"; j += String(rt ? rt->dbTrackCount : 0); j += ",";
    j += "\"playlistIndex\":"; j += String(rt ? rt->playlistIndex + 1 : 0); j += ",";
    j += "\"playlistCount\":"; j += String(rt ? rt->playlistCount : 0); j += ",";
    j += "\"playing\":"; j += (rt && rt->audioPlaying ? "true" : "false"); j += ",";
    j += "\"state\":\""; j += jsonEscape(rt ? rt->playerStateName : "NA"); j += "\",";
    j += "\"codec\":"; j += String(rt ? rt->mediaCodec : 0); j += ",";
    j += "\"sampleRate\":"; j += String(rt ? rt->mediaSampleRate : 0); j += ",";
    j += "\"channels\":"; j += String(rt ? rt->mediaChannels : 0); j += ",";
    j += "\"bits\":"; j += String(rt ? rt->mediaBits : 0); j += ",";
    j += "\"progress\":"; j += String(rt ? rt->mediaProgress : 0); j += ",";
    j += "\"elapsed\":"; j += String(seconds); j += ",";
    j += "\"duration\":"; j += String(duration); j += ",";
    j += "\"volume\":"; j += String(rt ? rt->volumePercent : 0); j += ",";
    j += "\"eqEnabled\":"; j += (rt && rt->eqEnabled ? "true" : "false"); j += ",";
    j += "\"eqBass\":"; j += String(rt ? rt->eqBassDb : 0); j += ",";
    j += "\"eqMid\":"; j += String(rt ? rt->eqMidDb : 0); j += ",";
    j += "\"eqTreble\":"; j += String(rt ? rt->eqTrebleDb : 0); j += ",";
    j += "\"eqPreset\":\""; j += jsonEscape(rt ? rt->eqPreset : "flat"); j += "\",";
    j += "\"health\":"; j += String(rt ? rt->audioHealth : 0); j += ",";
    j += "\"vuL\":"; j += String(rt ? rt->mediaVuLeft : 0); j += ",";
    j += "\"vuR\":"; j += String(rt ? rt->mediaVuRight : 0); j += ",";
    j += "\"underruns\":"; j += String(rt ? rt->mediaUnderruns : 0); j += ",";
    j += "\"shortWrites\":"; j += String(rt ? rt->mediaShortWrites : 0); j += ",";
    j += "\"title\":\""; j += jsonEscape(rt ? rt->mediaTitle : ""); j += "\",";
    j += "\"path\":\""; j += jsonEscape(rt ? rt->mediaPath : ""); j += "\",";
    if (rt && rt->systemMode == IQ200_MODE_LOCAL_PLAYER) {
      ArtworkCache& art = ArtworkCache::instance();
      // UI can hold the Artwork mutex while decoding a large JPEG/PNG. HTTP
      // status must never wait behind that decode: report a temporary fallback
      // and recover on the next poll instead of taking Web offline.
      ArtworkCache::ReadGuard artGuard(art, 0);
      const size_t artBytes = artGuard.locked() ? art.size() : 0;
      const bool artLimited = artGuard.locked() && artBytes > WEB_ARTWORK_MAX_BYTES;
      const bool artReady = artGuard.locked() && !artLimited &&
                            (art.state() == ArtworkCache::JPEG_READY || art.state() == ArtworkCache::PNG_READY) &&
                            art.data() && artBytes;
      j += "\"artworkReady\":"; j += (artReady ? "true" : "false"); j += ",";
      j += "\"artworkGeneration\":"; j += String(art.generation()); j += ",";
      j += "\"artworkBytes\":"; j += String((uint32_t)artBytes); j += ",";
      j += "\"artworkWebLimited\":"; j += (artLimited ? "true" : "false"); j += ",";
      j += "\"artworkUrl\":\""; if (artReady) { j += "/api/artwork?g="; j += String(art.generation()); } j += "\",";
    } else {
      // Do not instantiate the Artwork cache/mutex in the WebRadio platform.
      j += "\"artworkReady\":false,";
      j += "\"artworkGeneration\":0,";
      j += "\"artworkBytes\":0,";
      j += "\"artworkWebLimited\":false,";
      j += "\"artworkUrl\":\"\",";
    }
    j += "\"radioPlaying\":"; j += (radioSnapshot.active ? "true" : "false"); j += ",";
    j += "\"radioState\":\""; j += jsonEscape(rt ? rt->radioStatus : "IDLE"); j += "\",";
    j += "\"radioStation\":\""; j += jsonEscape(radioSnapshot.station); j += "\",";
    j += "\"radioTitle\":\""; j += jsonEscape(radioSnapshot.title); j += "\",";
    j += "\"radioError\":\""; j += jsonEscape(radioSnapshot.error); j += "\",";
    j += "\"radioCommandsQueued\":"; j += String(radioSnapshot.commandsQueued); j += ",";
    j += "\"radioCommandsDropped\":"; j += String(radioSnapshot.commandsDropped); j += ",";
    j += "\"radioConnectAttempts\":"; j += String(radioSnapshot.connectAttempts); j += ",";
    j += "\"radioReconnects\":"; j += String(radioSnapshot.reconnects); j += ",";
    j += "\"radioUnsupportedStreams\":"; j += String(radioSnapshot.unsupportedStreams); j += ",";
    j += "\"radioVuL\":"; j += String(rt ? rt->radioVuLeft : 0); j += ",";
    j += "\"radioVuR\":"; j += String(rt ? rt->radioVuRight : 0); j += ",";
    j += "\"radioVuTicks\":"; j += String(rt ? rt->radioVuTicks : 0); j += ",";
    j += "\"radioVuSegments\":"; j += String(rt ? rt->radioVuSegments : 24); j += ",";
    j += "\"radioVuFps\":"; j += String(rt ? rt->radioVuFps : 20); j += ",";
    j += "\"radioVuPeak\":"; j += (rt && rt->radioVuPeak ? "true" : "false"); j += ",";
    j += "\"radioVuHold\":"; j += String(rt ? rt->radioVuHoldMs : 450); j += ",";
    j += "\"radioVuDecay\":"; j += String(rt ? rt->radioVuDecay : 5); j += ",";
    j += "\"radioVuStyle\":"; j += String(rt ? rt->radioVuStyle : 2); j += ",";
    j += "\"radioVuGain\":"; j += String(rt ? rt->radioVuGain : 100); j += ",";
    j += "\"radioVuGate\":"; j += String(rt ? rt->radioVuGate : 2); j += ",";
    j += "\"displayBrightness\":"; j += String(rt ? rt->displayBrightness : 255); j += ",";
    j += "\"theme\":\""; j += jsonEscape(rt ? rt->activeTheme : "bluepro"); j += "\",";
    j += "\"radioStackHighWater\":"; j += String(radioSnapshot.taskStackHighWater); j += ",";
    j += "\"heap\":"; j += String(ESP.getFreeHeap()); j += ",";
    j += "\"psram\":"; j += String(ESP.getFreePsram()); j += ",";
    j += "\"core0Loops\":"; j += String(rt ? rt->core0Loops : 0); j += ",";
    j += "\"core1Loops\":"; j += String(rt ? rt->core1Loops : 0); j += ",";
    j += "\"frames\":"; j += String(rt ? rt->partialFrames : 0); j += ",";
    j += "\"queueDrops\":"; j += String(rt ? rt->eventQueueDrops : 0); j += ",";
    j += "\"navPreview\":"; j += (rt && rt->navPreviewEnabled ? "true" : "false"); j += ",";
    j += "\"navDelay\":"; j += String(rt ? rt->navCommitDelayMs : 0); j += ",";
    j += "\"requests\":"; j += String(requests); j += ",";
    j += "\"webTicks\":"; j += String(rt ? rt->webTicks : 0); j += ",";
    j += "\"webTickGapLastMs\":"; j += String(rt ? rt->webTickGapLastMs : 0); j += ",";
    j += "\"webTickGapMaxMs\":"; j += String(rt ? rt->webTickGapMaxMs : 0); j += ",";
    j += "\"webTaskRunning\":"; j += (rt && rt->webTaskRunning ? "true" : "false"); j += ",";
    j += "\"webTaskLoops\":"; j += String(rt ? rt->webTaskLoops : 0); j += ",";
    j += "\"webTaskStackHighWater\":"; j += String(rt ? rt->webTaskStackHighWater : 0); j += ",";
    j += "\"commandQueue\":"; j += String(commandCount); j += ",";
    j += "\"commandAccepted\":"; j += String(commandAccepted); j += ",";
    j += "\"commandRejected\":"; j += String(commandRejected); j += ",";
    j += "\"lastQueued\":\""; j += jsonEscape(lastQueued); j += "\",";
    j += "\"lastExecuted\":\""; j += jsonEscape(lastExecuted); j += "\"";
    j += "}";
    return j;
  }

  String wifiStatusJson() const {
    const wifi_mode_t mode = WiFi.getMode();
    const bool sta = WiFi.status() == WL_CONNECTED;
    const bool ap = mode == WIFI_AP || mode == WIFI_AP_STA;
    String j = "{";
    j.reserve(480);
    j += "\"ok\":true,";
    j += "\"mode\":\"";
    j += mode == WIFI_AP_STA ? "AP_STA" : mode == WIFI_AP ? "AP" : mode == WIFI_STA ? "STA" : "OFF";
    j += "\",";
    j += "\"status\":\""; j += jsonEscape(rt ? rt->wifiStatus : "NA"); j += "\",";
    j += "\"connected\":"; j += sta ? "true" : "false"; j += ",";
    j += "\"ap\":"; j += ap ? "true" : "false"; j += ",";
    j += "\"ssid\":\""; j += jsonEscape(sta ? WiFi.SSID().c_str() : ""); j += "\",";
    j += "\"savedSsid\":\""; j += jsonEscape(rt ? rt->wifiSavedSsid : ""); j += "\",";
    j += "\"ip\":\""; j += sta ? WiFi.localIP().toString() : String("0.0.0.0"); j += "\",";
    j += "\"apSsid\":\"IQ200-OS\",";
    j += "\"apIp\":\""; j += ap ? WiFi.softAPIP().toString() : String("0.0.0.0"); j += "\",";
    j += "\"rssi\":"; j += String(sta ? WiFi.RSSI() : 0); j += ",";
    j += "\"autoConnect\":"; j += (rt && rt->wifiAutoConnect) ? "true" : "false"; j += ",";
    j += "\"fallbackAp\":"; j += (rt && rt->wifiFallbackAp) ? "true" : "false"; j += ",";
    j += "\"reconnects\":"; j += String(rt ? rt->wifiReconnectCount : 0);
    j += "}";
    return j;
  }

  String modeStatusJson() const {
    String j = "{\"ok\":true,\"current\":\"";
    j += jsonEscape(rt ? rt->systemModeName : "UNKNOWN");
    j += "\",\"currentId\":" + String(rt ? rt->systemMode : 0);
    j += ",\"healthy\":" + String(rt && rt->modeBootHealthy ? "true" : "false");
    j += ",\"modes\":[";
    j += "{\"id\":0,\"key\":\"center\",\"name\":\"Mode Center\",\"available\":true},";
    j += "{\"id\":1,\"key\":\"local\",\"name\":\"Local Player\",\"available\":true},";
    j += "{\"id\":2,\"key\":\"webradio\",\"name\":\"WebRadio\",\"available\":true},";
    j += "{\"id\":3,\"key\":\"bluetooth\",\"name\":\"Bluetooth\",\"available\":false},";
    j += "{\"id\":4,\"key\":\"radio\",\"name\":\"FM / Radio\",\"available\":false}]}";
    return j;
  }

  String wifiScanJson(bool startScan) {
    if (startScan && !wifiScanActive) {
      WiFi.scanDelete();
      const int rc = WiFi.scanNetworks(true, true);
      wifiScanActive = rc == WIFI_SCAN_RUNNING;
    }
    int n = WiFi.scanComplete();
    if (n == WIFI_SCAN_RUNNING) return "{\"ok\":true,\"running\":true,\"networks\":[]}";
    if (n < 0) {
      wifiScanActive = false;
      return "{\"ok\":false,\"running\":false,\"networks\":[]}";
    }
    String j = "{\"ok\":true,\"running\":false,\"networks\":[";
    j.reserve(256 + n * 96);
    for (int i = 0; i < n; ++i) {
      if (i) j += ',';
      j += "{\"ssid\":\""; j += jsonEscape(WiFi.SSID(i).c_str()); j += "\",";
      j += "\"rssi\":"; j += String(WiFi.RSSI(i)); j += ',';
      j += "\"channel\":"; j += String(WiFi.channel(i)); j += ',';
      j += "\"open\":"; j += WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "true" : "false";
      j += "}";
    }
    j += "]}";
    WiFi.scanDelete();
    wifiScanActive = false;
    return j;
  }

  String trackTitle(const String& path) const {
    int slash = path.lastIndexOf('/');
    String title = slash >= 0 ? path.substring(slash + 1) : path;
    int dot = title.lastIndexOf('.');
    if (dot > 0) title.remove(dot);
    return title;
  }

  String trackCodec(const String& path) const {
    String x = path;
    x.toLowerCase();
    if (x.endsWith(".flac")) return "FLAC";
    if (x.endsWith(".mp3")) return "MP3";
    if (x.endsWith(".wav")) return "WAV";
    if (x.endsWith(".ogg")) return "OGG";
    if (x.endsWith(".opus")) return "OPUS";
    if (x.endsWith(".aac") || x.endsWith(".m4a")) return "AAC";
    return "AUDIO";
  }

  String libraryJson(int offset, int limit, String query) const {
    if (!playlist) return R"({"ok":false,"error":"playlist_unavailable","items":[]})";
    offset = max(0, offset);
    limit = constrain(limit, 1, 100);
    query.trim(); query.toLowerCase();
    String j = "{\"ok\":true,\"total\":" + String(playlist->size()) +
               ",\"current\":" + String(playlist->index()) +
               ",\"offset\":" + String(offset) + ",\"limit\":" + String(limit) +
               ",\"items\":[";
    j.reserve(512 + limit * 150);
    int matched = 0, emitted = 0;
    for (int i = 0; i < playlist->size(); ++i) {
      const String& path = playlist->at(i);
      String title = trackTitle(path);
      String hay = path; hay.toLowerCase();
      if (query.length() && hay.indexOf(query) < 0) continue;
      if (matched++ < offset) continue;
      if (emitted >= limit) continue;
      if (emitted++) j += ',';
      j += "{\"index\":" + String(i) + ",\"number\":" + String(i + 1) +
           ",\"current\":" + String(i == playlist->index() ? "true" : "false") +
           ",\"title\":\"" + jsonEscape(title.c_str()) +
           "\",\"codec\":\"" + trackCodec(path) +
           "\",\"path\":\"" + jsonEscape(path.c_str()) + "\"}";
    }
    j += "],\"matched\":" + String(matched) + "}";
    return j;
  }

  String commandsJson() const {
    String j = "{\"commands\":[";
    if (commands) {
      const size_t n = commands->count();
      for (size_t i = 0; i < n; ++i) {
        const CommandManager::Entry* e = commands->get(i);
        if (!e) continue;
        if (i) j += ',';
        j += "{\"category\":\""; j += jsonEscape(e->category); j += "\",";
        j += "\"command\":\""; j += jsonEscape(e->command); j += "\",";
        j += "\"aliases\":\""; j += jsonEscape(e->aliases); j += "\",";
        j += "\"description\":\""; j += jsonEscape(e->description); j += "\"}";
      }
    }
    j += "]}";
    return j;
  }

  bool enqueueCommand(String command) {
    command.trim();
    if (!command.length() || command.length() > COMMAND_MAX_LEN) {
      commandRejected++;
      return false;
    }
    bool ok = false;
    portENTER_CRITICAL(&commandMux);
    if (commandCount < COMMAND_QUEUE_SIZE) {
      strncpy(commandQueue[commandTail], command.c_str(), COMMAND_MAX_LEN);
      commandQueue[commandTail][COMMAND_MAX_LEN] = 0;
      commandTail = (uint8_t)((commandTail + 1) % COMMAND_QUEUE_SIZE);
      commandCount++;
      commandAccepted++;
      strncpy(lastQueued, command.c_str(), COMMAND_MAX_LEN);
      lastQueued[COMMAND_MAX_LEN] = 0;
      ok = true;
    } else {
      commandRejected++;
    }
    portEXIT_CRITICAL(&commandMux);
    return ok;
  }

  void sendJson(int code, const String& body) {
    requests++;
    if (rt) {
      rt->webRequests = requests;
      rt->webLastRequestMs = millis();
    }
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Cache-Control", "no-store");
    server.send(code, "application/json; charset=utf-8", body);
  }

  String radioStationsJson() const {
    String j="{\"ok\":true,\"count\":"+String(radioStations.count())+",\"max\":"+String(RadioStationStore::MAX_STATIONS)+",\"stations\":[";
    for(uint8_t i=0;i<radioStations.count();++i){
      const RadioStation* s=radioStations.get(i); if(!s) continue; if(i) j+=',';
      j+="{\"id\":"+String(i)+",\"name\":\""+jsonEscape(s->name.c_str())+"\",\"url\":\""+jsonEscape(s->url.c_str())+"\",\"genre\":\""+jsonEscape(s->genre.c_str())+"\",\"country\":\""+jsonEscape(s->country.c_str())+"\",\"artwork\":\""+jsonEscape(s->artwork.c_str())+"\",\"favorite\":"+(s->favorite?"true":"false")+"}";
    }
    j+="]}"; return j;
  }

  String radioExportJson() const {
    String j="[";
    for(uint8_t i=0;i<radioStations.count();++i){ const RadioStation* s=radioStations.get(i); if(!s) continue; if(i) j+=',';
      j+="{\"name\":\""+jsonEscape(s->name.c_str())+"\",\"url\":\""+jsonEscape(s->url.c_str())+"\",\"genre\":\""+jsonEscape(s->genre.c_str())+"\",\"country\":\""+jsonEscape(s->country.c_str())+"\",\"artwork\":\""+jsonEscape(s->artwork.c_str())+"\",\"favorite\":"+(s->favorite?"true":"false")+"}";
    } j+="]"; return j;
  }

  String radioExportM3u() const {
    String out="#EXTM3U\n";
    for(uint8_t i=0;i<radioStations.count();++i){ const RadioStation* s=radioStations.get(i); if(!s) continue; out+="#EXTINF:-1,"+s->name+"\n"+s->url+"\n"; }
    return out;
  }

  void setupRoutes() {
    server.on("/", HTTP_GET, [this]() {
      requests++;
      if (rt) { rt->webRequests = requests; rt->webLastRequestMs = millis(); }
      static const char PAGE[] PROGMEM = R"HTML(<!doctype html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>IQ200 OS Web UI 2.0</title><style>
:root{color-scheme:dark;--bg:#071018;--panel:#101c26;--line:#294152;--text:#edf7ff;--muted:#91a7b7;--accent:#4fc3ff;--ok:#58d68d;--warn:#ffd166}*{box-sizing:border-box}body{font-family:system-ui,sans-serif;background:linear-gradient(135deg,#071018,#0d1822);color:var(--text);margin:0;min-height:100vh}.top{position:sticky;top:0;z-index:5;background:#08131dd9;backdrop-filter:blur(10px);border-bottom:1px solid var(--line);padding:12px 16px;display:flex;align-items:center;gap:12px}.brand{font-weight:800;letter-spacing:.4px}.tabs{display:flex;gap:6px;overflow:auto;margin-left:auto}.tab{background:transparent;border:1px solid transparent;color:var(--muted);padding:8px 12px;border-radius:9px;cursor:pointer}.tab.active{background:#163147;color:#fff;border-color:#315873}.wrap{max-width:1200px;margin:auto;padding:16px}.view{display:none}.view.active{display:block}.grid{display:grid;grid-template-columns:repeat(12,1fr);gap:14px}.card{background:var(--panel);border:1px solid var(--line);border-radius:14px;padding:15px;box-shadow:0 8px 25px #0004}.span4{grid-column:span 4}.span5{grid-column:span 5}.span7{grid-column:span 7}.span8{grid-column:span 8}.span12{grid-column:span 12}h2,h3{margin:0 0 12px}.art{aspect-ratio:1;max-width:300px;margin:auto;background:#071018;border:2px solid #496476;border-radius:14px;display:grid;place-items:center;overflow:hidden}.art img{width:100%;height:100%;object-fit:contain;display:none}.art .fallback{font-size:24px;color:#9fb2bf}.title{font-size:clamp(21px,3vw,34px);font-weight:750;overflow-wrap:anywhere}.path{color:var(--muted);font-size:12px;overflow-wrap:anywhere}.meta{display:flex;gap:7px;flex-wrap:wrap;margin:10px 0}.pill{padding:5px 9px;background:#162a39;border:1px solid #2e4b60;border-radius:999px;font-size:12px}.progress{height:10px;background:#071018;border-radius:999px;overflow:hidden;border:1px solid #263f51}.progress>i{display:block;height:100%;background:linear-gradient(90deg,#36b9f1,#6ce5bd);width:0}.times{display:flex;justify-content:space-between;color:var(--muted);font:12px ui-monospace,monospace;margin-top:5px}.controls{display:flex;justify-content:center;gap:10px;flex-wrap:wrap;margin-top:14px}button{background:#173246;color:#fff;border:1px solid #3b617a;border-radius:10px;padding:10px 14px;cursor:pointer}button:hover{background:#24516d}.primary{font-size:20px;min-width:64px}.vurow{display:grid;grid-template-columns:22px 1fr;gap:7px;align-items:center;margin:9px 0}.vu{height:17px;background:#071018;border:1px solid #284152;border-radius:4px;overflow:hidden}.vu i{height:100%;display:block;width:0;background:linear-gradient(90deg,#49d17d 0 65%,#ffd166 66% 85%,#ff6b6b 86%)}input,select{width:100%;background:#08131d;color:#fff;border:1px solid #3a566a;border-radius:9px;padding:10px}.row{display:flex;gap:9px;flex-wrap:wrap}.row>*{flex:1}.metric{font:13px ui-monospace,monospace;display:grid;grid-template-columns:1fr auto;gap:8px;padding:7px 0;border-bottom:1px solid #203440}.ok{color:var(--ok)}.warn{color:var(--warn)}.cmdline{display:flex;gap:8px}.cmdline input{flex:1;font-family:ui-monospace,monospace}.toast{min-height:24px;color:var(--warn);margin-top:8px}.search{margin-bottom:10px}.command{display:grid;grid-template-columns:minmax(190px,1fr) 2fr auto;gap:8px;align-items:center;border-top:1px solid #243542;padding:8px 0}.command code{color:#9be18d;overflow-wrap:anywhere}.desc{font-size:13px;color:#b9c7d1}.alias{font-size:11px;color:#7e95a5}.cat{color:#78c7ff;margin:16px 0 5px}.quickgrid{display:grid;grid-template-columns:repeat(2,1fr);gap:8px}.track{display:grid;grid-template-columns:54px minmax(0,1fr) auto;gap:10px;align-items:center;padding:10px 4px;border-top:1px solid #243542}.track.current{background:#143047;border-radius:9px;padding:10px}.tracknum{color:var(--muted);text-align:center}.tracktitle{font-weight:700;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}.trackpath{font-size:11px;color:var(--muted);overflow:hidden;text-overflow:ellipsis;white-space:nowrap}.pager{display:flex;justify-content:space-between;align-items:center;gap:10px;margin-top:12px}.nowplaylist{margin-top:14px}.nowplaylist-head{display:flex;gap:9px;align-items:center;position:sticky;top:74px;z-index:3;background:var(--panel);padding:4px 0 10px}.nowplaylist-head input{flex:1}.nowtracklist{max-height:58vh;min-height:280px;overflow:auto;overscroll-behavior:contain;border:1px solid #243b4b;border-radius:11px;background:#0a151e}.nowtrack{display:grid;grid-template-columns:50px minmax(0,1fr) 72px;gap:10px;align-items:center;padding:9px 10px;border-bottom:1px solid #1f3341;cursor:pointer}.nowtrack:hover{background:#102838}.nowtrack.current{background:#15394f;border-left:4px solid var(--accent);padding-left:6px}.nowtrack.current .tracktitle{color:#fff}.nowtrack .playmark{color:var(--accent);font-weight:900}.nowplaylist-foot{display:flex;justify-content:space-between;gap:10px;color:var(--muted);font-size:12px;padding:9px 2px}.now-empty{padding:24px;color:var(--muted);text-align:center}.station{display:grid;grid-template-columns:minmax(0,1fr) minmax(0,2fr) auto;gap:10px;align-items:center;padding:10px 4px;border-top:1px solid #243542}.stationname{font-weight:750}.stationmeta{font-size:11px;color:var(--muted)}.danger{background:#51242b;border-color:#8a3b46}.importbox{min-height:150px;width:100%;background:#08131d;color:#fff;border:1px solid #3a566a;border-radius:9px;padding:10px;font-family:ui-monospace,monospace}@media(max-width:850px){.span4,.span5,.span7,.span8{grid-column:span 12}.top{align-items:flex-start;flex-direction:column}.tabs{margin-left:0;width:100%}.command{grid-template-columns:1fr auto}.desc{grid-column:1/-1}}
</style></head><body><header class="top"><div class="brand">IQ200 OS · Web UI 2.0</div><nav class="tabs"><button class="tab" data-view="modes">Mode Center</button><button class="tab active" data-view="player">Now Playing</button><button class="tab" data-view="library">Library</button><button class="tab" data-view="webradio">Web Radio</button><button class="tab" data-view="wifi">WiFi</button><button class="tab" data-view="appearance">Appearance</button><button class="tab" data-view="diagnostics">Diagnostics</button><button class="tab" data-view="console">Console</button></nav></header><main class="wrap">
<section id="modes" class="view"><div class="grid"><div class="card span12"><h2>Mode Center · Clean Boot</h2><p class="path">Switching saves the selected platform in NVS, stops active services and performs a full reboot. Only the selected platform is initialized after boot.</p><div id="modeNow" class="toast">Loading mode…</div><div class="quickgrid"><button data-mode="center">Mode Center</button><button data-mode="local">Local Player</button><button data-mode="webradio">WebRadio</button><button disabled>Bluetooth · Future</button><button disabled>FM / Radio · Future</button></div><div id="modeInfo" class="toast"></div></div></div></section>
<section id="player" class="view active"><div class="grid"><div class="card span4"><div class="art"><img id="artwork" alt="Album artwork"><div id="artFallback" class="fallback">CD</div></div></div><div class="card span8"><div id="title" class="title">No track</div><div id="path" class="path"></div><div id="meta" class="meta"></div><div class="progress"><i id="bar"></i></div><div class="times"><span id="elapsed">00:00</span><span id="duration">00:00</span></div><div class="controls"><button class="primary" data-cmd="prev">⏮</button><button class="primary" data-cmd="play">▶</button><button class="primary" data-cmd="stop">■</button><button class="primary" data-cmd="next">⏭</button></div><p>Volume <b id="volv">8%</b></p><input id="vol" type="range" min="0" max="100" value="8"></div><div class="card span12 nowplaylist"><div class="nowplaylist-head"><input id="nowTrackSearch" placeholder="Search playlist"><button id="nowTrackRefresh">Refresh</button></div><div id="nowTrackInfo" class="toast"></div><div id="nowTrackList" class="nowtracklist"><div class="now-empty">Loading playlist…</div></div><div class="nowplaylist-foot"><span id="nowTrackPage">Tracks</span><span id="nowTrackPosition">Playing —</span></div></div></div></section>
<section id="library" class="view"><div class="grid"><div class="card span12"><h2>Music Library</h2><div class="row"><input id="trackSearch" placeholder="Search track or folder"><button id="trackRefresh">Refresh</button></div><div id="trackInfo" class="toast"></div><div id="trackList">Loading…</div><div class="pager"><button id="trackPrevPage">Previous</button><span id="trackPage">Page 1</span><button id="trackNextPage">Next</button></div></div></div></section>
<section id="webradio" class="view"><div class="grid"><div class="card span12"><h2>WebRadio Player</h2><div style="display:grid;grid-template-columns:minmax(150px,220px) minmax(0,1fr);gap:20px;align-items:center"><div class="art" style="width:100%;max-width:220px"><img id="radioPlayerArtwork" alt="Station artwork"><div id="radioPlayerFallback" class="fallback">RADIO</div></div><div><div id="radioPlayerStation" class="title">Select a station</div><div id="radioPlayerTitle" style="font-size:18px;color:var(--accent);min-height:28px;margin-top:8px">—</div><div id="radioPlayerState" class="pill" style="display:inline-block;margin-top:10px">IDLE</div><div class="vurow"><b>L</b><div class="vu"><i id="radioPlayerVuL"></i></div></div><div class="vurow"><b>R</b><div class="vu"><i id="radioPlayerVuR"></i></div></div><div class="controls"><button id="radioPrev" class="primary">⏮</button><button id="radioPlay" class="primary">▶</button><button id="radioStop" class="primary danger">■</button><button id="radioNext" class="primary">⏭</button></div><p>Volume <b id="radioVolV">8%</b></p><input id="radioVol" type="range" min="0" max="100" value="8"><div id="radioNow" class="toast">WebRadio idle</div></div></div></div><div class="card span7"><h2>Web Radio Stations</h2><div class="row"><input id="radioSearch" placeholder="Search station, genre or country"><button id="radioRefresh">Refresh</button></div><div id="radioInfo" class="toast"></div><div class="controls"><button id="radioStatusBtn">Refresh status</button></div><div id="radioList">Loading…</div></div><div class="card span5"><h2>Station Editor</h2><input id="radioId" type="hidden" value="-1"><label>Name</label><input id="radioName" maxlength="80" placeholder="Radio ROKS"><label>Stream URL</label><input id="radioUrl" maxlength="240" placeholder="https://stream.example/radio.mp3"><label>Station cover URL (JPEG/PNG, max 256 KB)</label><input id="radioArtwork" maxlength="240" placeholder="https://example.com/station.jpg"><div class="row"><div><label>Genre</label><input id="radioGenre" maxlength="40" placeholder="Rock"></div><div><label>Country</label><input id="radioCountry" maxlength="20" placeholder="UA"></div></div><label><input id="radioFavorite" type="checkbox" style="width:auto"> Favorite</label><div class="controls"><button id="radioSave">Save station</button><button id="radioNew">New</button><button id="radioDelete" class="danger">Delete</button></div><div id="radioEditInfo" class="toast"></div></div><div class="card span12"><h2>Import / Export</h2><p class="path">Import M3U or text: one URL per line, #EXTINF M3U, TSV, or name;url;genre;country.</p><textarea id="radioImport" class="importbox" placeholder="#EXTM3U
#EXTINF:-1,Radio name
https://stream.example/live"></textarea><div class="controls"><button id="radioImportAdd">Import + append</button><button id="radioImportReplace" class="danger">Replace all</button><button id="radioExportJson">Export JSON</button><button id="radioExportM3u">Export M3U</button></div></div></div></section>
<section id="wifi" class="view"><div class="grid"><div class="card span5"><h2>WiFi Foundation</h2><div id="wifiMetrics"></div><div class="controls"><button id="wifiRefresh">Refresh</button><button id="wifiScan">Scan networks</button><button id="wifiReconnect">Reconnect saved</button><button id="wifiDisconnect">Disconnect STA</button><button id="wifiAutoOn">Auto ON</button><button id="wifiAutoOff">Auto OFF</button><button id="wifiFallbackOn">Fallback ON</button><button id="wifiFallbackOff">Fallback OFF</button><button id="wifiForget">Forget profile</button></div><div id="wifiToast" class="toast"></div></div><div class="card span7"><h2>Connect while AP stays active</h2><label>Network</label><select id="wifiSsid"><option value="">Scan first or enter SSID below</option></select><label>SSID</label><input id="wifiSsidManual" maxlength="32" placeholder="WiFi name"><label>Password</label><input id="wifiPassword" maxlength="64" type="password" placeholder="Leave empty for open network"><div class="controls"><button id="wifiConnect">Connect AP+STA</button><button id="wifiSave">Save profile</button></div><h3>Nearby networks</h3><div id="wifiNetworks" class="path">Not scanned</div></div></div></section><section id="appearance" class="view"><div class="grid"><div class="card span5"><h2>Theme</h2><div class="quickgrid"><button data-cmd="theme bluepro">Blue Pro</button><button data-cmd="theme emerald">Emerald Studio</button><button data-cmd="theme amber">Amber Vintage</button><button data-cmd="theme purple">Purple Neon</button><button data-cmd="theme ice">Ice White</button><button data-cmd="theme oled">OLED Black</button><button data-cmd="theme carbon">Carbon Red</button><button data-cmd="theme matrix">Matrix</button></div><p class="path">Theme is saved and applied to Local Player or WebRadio TFT.</p></div><div class="card span7"><h2>Display & Volume</h2><p>Volume <b id="settingsVolV">8%</b></p><input id="settingsVol" type="range" min="0" max="100" value="8"><p>Brightness <b id="brightnessV">255</b></p><input id="brightness" type="range" min="5" max="255" value="255"><div id="settingsInfo" class="toast"></div></div><div class="card span7 local-only"><h2>Player UI 2.0</h2><div class="row"><select id="playerLayout"><option value="artwork">Artwork Focus 480x320</option></select><button id="applyPlayerLayout">Apply layout</button></div><p class="path">Approved fixed 480×320 Artwork Focus layout with dual L/R VU and time labels.</p></div><div class="card span7"><h2>VU PRO</h2><div class="row"><select id="vustyle"><option>line</option><option>thin</option><option>rect</option><option>dot</option><option>neon</option><option>center</option></select><button id="applyStyle">Apply style</button></div><p>Segments <b id="segv">24</b></p><input id="segments" type="range" min="4" max="24" value="24"><p>FPS <b id="fpsv">20</b></p><input id="vufps" type="range" min="10" max="30" value="20"><div class="row"><div><p>Peak hold <b id="holdv">450 ms</b></p><input id="vuhold" type="range" min="50" max="1500" step="50" value="450"></div><div><p>Decay <b id="decayv">5</b></p><input id="vudecay" type="range" min="1" max="10" value="5"></div></div><div class="row"><div><p>VU level <b id="gainv">100%</b></p><input id="vugain" type="range" min="50" max="300" step="10" value="100"></div><div><p>Noise gate <b id="gatev">2</b></p><input id="vugate" type="range" min="0" max="30" value="2"></div></div><div class="row" style="margin-top:10px"><button data-cmd="vu peak on">Peak On</button><button data-cmd="vu peak off">Peak Off</button><button data-cmd="vu status">VU Status</button></div></div><div class="card span12"><h2>Equalizer</h2><div class="row"><select id="eqPreset"><option value="flat">Flat</option><option value="rock">Rock</option><option value="pop">Pop</option><option value="jazz">Jazz</option><option value="classic">Classic</option><option value="bass">Bass Boost</option><option value="treble">Treble Boost</option><option value="vocal">Vocal</option><option value="off">Off</option><option value="custom">Custom</option></select><button id="applyEqPreset">Apply preset</button><button id="eqStatus">EQ status</button></div><div class="grid" style="margin-top:14px"><div class="card span4"><p>Bass <b id="eqBassV">0 dB</b></p><input id="eqBass" type="range" min="-12" max="12" value="0"></div><div class="card span4"><p>Mid <b id="eqMidV">0 dB</b></p><input id="eqMid" type="range" min="-12" max="12" value="0"></div><div class="card span4"><p>Treble <b id="eqTrebleV">0 dB</b></p><input id="eqTreble" type="range" min="-12" max="12" value="0"></div></div><div class="row" style="margin-top:10px"><button id="applyEqCustom">Apply custom EQ</button><button data-cmd="eq flat">Reset Flat</button><button data-cmd="eq off">Disable EQ</button></div><p class="path">DSP path: Decoder → EQ → Volume → I2S. Range: -12…+12 dB.</p></div><div class="card span12 local-only"><h2>SD Clock</h2><div class="quickgrid"><button data-cmd="sd speed 16">16 MHz · Performance</button><button data-cmd="sd speed 12">12 MHz · Recommended</button><button data-cmd="sd speed 10">10 MHz · Stability</button><button data-cmd="sd status">SD status</button></div><p class="path">Stop playback before changing the SD clock. The selected value is saved for the next boot.</p></div><div class="card span12 local-only"><h2>Navigation</h2><div class="row"><button data-cmd="nav preview on">Preview On</button><button data-cmd="nav preview off">Preview Off</button><input id="navdelay" type="number" min="200" max="1500" value="450"><button id="applyNav">Set delay</button></div></div></div></section>
<section id="diagnostics" class="view"><div class="grid"><div class="card span4"><h2>Audio</h2><div id="audioMetrics"></div></div><div class="card span4"><h2>System</h2><div id="systemMetrics"></div></div><div class="card span4"><h2>Transport</h2><div id="transportMetrics"></div></div><div class="card span12"><h2>Quick diagnostics</h2><div class="row"><button data-cmd="perf">Performance</button><button data-cmd="ui">UI profiler</button><button data-cmd="bb status">Black Box</button><button data-cmd="health">Health</button><button data-cmd="status">Status</button><button data-cmd="pipeline">Pipeline</button></div></div></div></section>
<section id="console" class="view"><div class="grid"><div class="card span12"><h2>Command console</h2><div class="cmdline"><input id="cmd" placeholder="Enter any IQ200 command"><button id="send">Run</button></div><div id="toast" class="toast"></div></div><div class="card span12"><h2>All commands</h2><input id="search" class="search" placeholder="Search commands, aliases, descriptions"><div id="commands">Loading…</div></div></div></section>
</main><script>
const $=id=>document.getElementById(id);let all=[];let eqEditing=false,eqPendingUntil=0,eqPending=null;let vuStyleEditing=false,vuStylePending='',vuStylePendingUntil=0;let trackOffset=0,trackLimit=50,trackMatched=0,trackQuery="";let nowOffset=0,nowLimit=50,nowQuery="",nowCurrent=-1,nowLoadedCurrent=-2,nowUserScrolling=false,nowScrollTimer=0;
const TRACK_RESUME_KEY='iq200.web.library.resume.v1';
let trackRestoreScroll=0;
function loadTrackResume(){try{const x=JSON.parse(localStorage.getItem(TRACK_RESUME_KEY)||'{}');trackOffset=Math.max(0,Number(x.offset)||0);trackQuery=String(x.query||'');trackRestoreScroll=Math.max(0,Number(x.scrollY)||0);if($('trackSearch'))$('trackSearch').value=trackQuery}catch(e){}}
function saveTrackResume(){try{localStorage.setItem(TRACK_RESUME_KEY,JSON.stringify({offset:trackOffset,query:($('trackSearch')?.value||trackQuery||'').trim(),scrollY:window.scrollY||0,ts:Date.now()}))}catch(e){}}
function restoreTrackScroll(){if(trackRestoreScroll>0){requestAnimationFrame(()=>{window.scrollTo(0,trackRestoreScroll);trackRestoreScroll=0})}}const esc=s=>String(s??'').replace(/[&<>"']/g,m=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[m]));
function fmt(t){t=Math.max(0,Number(t)||0);return String(Math.floor(t/60)).padStart(2,'0')+':'+String(t%60).padStart(2,'0')}
async function fetchJson(url,options={},timeout=1800){const controller=new AbortController();const timer=setTimeout(()=>controller.abort(),timeout);try{const r=await fetch(url,{...options,signal:controller.signal});return await r.json()}finally{clearTimeout(timer)}}
function showView(id){document.querySelectorAll('.tab,.view').forEach(x=>x.classList.remove('active'));document.querySelector(`[data-view="${id}"]`)?.classList.add('active');$(id)?.classList.add('active')}
let modeInitialViewDone=false;
async function modeStatus(){try{const m=await fetchJson('/api/mode',{cache:'no-store'},1500);$('modeNow').textContent=`Active: ${m.current||'UNKNOWN'} · clean boot ${m.healthy?'healthy':'checking'}`;const local=Number(m.currentId)===1,wr=Number(m.currentId)===2;for(const id of ['player','library']){const tab=document.querySelector(`[data-view="${id}"]`);if(tab)tab.hidden=!local}const appearanceTab=document.querySelector('[data-view="appearance"]');if(appearanceTab)appearanceTab.hidden=!(local||wr);document.querySelectorAll('.local-only').forEach(x=>x.hidden=!local);const radioTab=document.querySelector('[data-view="webradio"]');if(radioTab)radioTab.hidden=!wr;if(!modeInitialViewDone){modeInitialViewDone=true;showView(wr?'webradio':local?'player':'modes');if(local){loadTrackResume();loadNowResume();loadNowPlaylist(true)}else if(wr){loadRadioStations();updateRadioStatus()}}}catch(e){$('modeNow').textContent='Mode status unavailable: '+e}}
async function switchMode(mode){$('modeInfo').textContent='Saving mode and preparing clean reboot…';document.querySelectorAll('[data-mode]').forEach(b=>b.disabled=true);try{const d=await fetchJson('/api/mode/switch',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams({mode})},1800);$('modeInfo').textContent=d.alreadyActive?'Already active: '+d.current:(d.ok?'Rebooting to '+d.rebootingTo:(d.error||'Mode switch failed'));if(d.alreadyActive||!d.ok)document.querySelectorAll('[data-mode]').forEach(b=>b.disabled=false)}catch(e){$('modeInfo').textContent='Device is rebooting…'}}
async function run(cmd){cmd=(cmd||'').trim();if(!cmd)return;const isEq=cmd==='eq'||cmd.startsWith('eq ');if(isEq){eqPendingUntil=Date.now()+2500;if(cmd.startsWith('eq custom ')){const a=cmd.split(/\s+/);eqPending={preset:'custom',bass:Number(a[2])||0,mid:Number(a[3])||0,treble:Number(a[4])||0}}else if(cmd!=='eq'&&cmd!=='eq status'&&cmd!=='eq list'){eqPending={preset:cmd.substring(3).trim()}}}try{const body=new URLSearchParams({cmd});const r=await fetch('/api/command',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});const j=await r.json();$('toast').textContent=j.ok?'Queued: '+cmd:'Rejected: '+(j.error||'unknown');if(j.ok&&$('cmd'))$('cmd').value='';setTimeout(update,100)}catch(e){$('toast').textContent='Offline: '+e}}
function metric(k,v,cls=''){return `<div class="metric"><span>${esc(k)}</span><b class="${cls}">${esc(v)}</b></div>`}
let artworkGeneration=-1;

let radioCache=[],radioCurrentId=-1;
async function loadRadioStations(){try{const d=await fetchJson('/api/radio/stations',{cache:'no-store'},3000);radioCache=d.stations||[];renderRadioStations()}catch(e){$('radioInfo').textContent='Station API timeout: '+e}}
function renderRadioStations(){const q=($('radioSearch').value||'').toLowerCase();const list=radioCache.filter(s=>!q||(s.name+' '+s.genre+' '+s.country+' '+s.url).toLowerCase().includes(q));$('radioInfo').textContent=`${list.length} / ${radioCache.length} stations · stored in ESP Flash (NVS)`;$('radioList').innerHTML=list.length?list.map(s=>`<div class="station"><div><div class="stationname">${s.artwork?`<img src="${esc(s.artwork)}" loading="lazy" style="width:42px;height:42px;object-fit:cover;border-radius:7px;vertical-align:middle;margin-right:8px" onerror="this.style.display='none'">`:''}${s.favorite?'★ ':''}${esc(s.name)}</div><div class="stationmeta">${esc(s.genre||'—')} · ${esc(s.country||'—')}</div></div><div class="path">${esc(s.url)}</div><div class="controls"><button onclick="playRadio(${s.id})">Play</button><button onclick="editRadio(${s.id})">Edit</button></div></div>`).join(''):'<div class="now-empty">No stations</div>'}
function editRadio(id){const s=radioCache.find(x=>x.id===id);if(!s)return;$('radioId').value=id;$('radioName').value=s.name||'';$('radioUrl').value=s.url||'';$('radioGenre').value=s.genre||'';$('radioCountry').value=s.country||'';$('radioArtwork').value=s.artwork||'';$('radioFavorite').checked=!!s.favorite;$('radioEditInfo').textContent='Editing station #'+(id+1)}
function clearRadioEditor(){$('radioId').value=-1;$('radioName').value='';$('radioUrl').value='';$('radioGenre').value='';$('radioCountry').value='';$('radioArtwork').value='';$('radioFavorite').checked=false;$('radioEditInfo').textContent='New station'}
async function saveRadio(){const b=new URLSearchParams({id:$('radioId').value,name:$('radioName').value,url:$('radioUrl').value,genre:$('radioGenre').value,country:$('radioCountry').value,artwork:$('radioArtwork').value,favorite:$('radioFavorite').checked?'1':'0'});const r=await fetch('/api/radio/station',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:b});const d=await r.json();$('radioEditInfo').textContent=d.ok?'Saved in ESP memory':(d.error||'Save failed');if(d.ok){clearRadioEditor();loadRadioStations()}}
async function deleteRadio(){const id=Number($('radioId').value);if(id<0)return;$('radioEditInfo').textContent='Deleting…';const r=await fetch('/api/radio/station/delete',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams({id})});const d=await r.json();$('radioEditInfo').textContent=d.ok?'Deleted':(d.error||'Delete failed');if(d.ok){clearRadioEditor();loadRadioStations()}}
async function importRadio(replace){const text=$('radioImport').value;if(!text.trim())return;const r=await fetch('/api/radio/import',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams({text,replace:replace?'1':'0'})});const d=await r.json();$('radioEditInfo').textContent=d.ok?`Imported ${d.added} stations`:(d.error||'Import failed');if(d.ok)loadRadioStations()}
function exportRadio(fmt){window.location='/api/radio/export?format='+fmt}
async function playRadio(id){id=Number(id);if(id<0||id>=radioCache.length)return;radioCurrentId=id;$('radioNow').textContent='Queued…';const selected=radioCache.find(s=>Number(s.id)===id);if(selected){$('radioPlayerStation').textContent=selected.name||'Station';setRadioArtwork(selected.artwork||'')}try{const d=await fetchJson('/api/radio/play',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams({id})},1200);$('radioNow').textContent=d.ok?'Playback queued':(d.error||'Play failed');setTimeout(updateRadioStatus,250)}catch(e){$('radioNow').textContent='Play API timeout: '+e}}
async function stopRadio(){try{const d=await fetchJson('/api/radio/stop',{method:'POST'},1200);$('radioNow').textContent=d.ok?'Stop queued':(d.error||'Stop failed');setTimeout(updateRadioStatus,200)}catch(e){$('radioNow').textContent='Stop API timeout: '+e}}
function setRadioArtwork(url){const img=$('radioPlayerArtwork'),fb=$('radioPlayerFallback');if(url){img.onload=()=>{img.style.display='block';fb.style.display='none'};img.onerror=()=>{img.style.display='none';fb.style.display='block'};if(img.src!==url)img.src=url}else{img.removeAttribute('src');img.style.display='none';fb.style.display='block'}}
function stepRadio(direction){if(!radioCache.length)return;let pos=radioCache.findIndex(s=>Number(s.id)===radioCurrentId);if(pos<0)pos=direction>0?-1:0;pos=(pos+direction+radioCache.length)%radioCache.length;playRadio(Number(radioCache[pos].id))}
let radioStatusInFlight=false;async function updateRadioStatus(){if(radioStatusInFlight)return;radioStatusInFlight=true;try{const d=await fetchJson('/api/radio/status',{cache:'no-store'},1500);const match=radioCache.find(s=>s.name===d.station);if(match)radioCurrentId=Number(match.id);$('radioPlayerStation').textContent=d.station||'Select a station';$('radioPlayerTitle').textContent=d.title||'—';$('radioPlayerState').textContent=d.state||'IDLE';$('radioPlayerVuL').style.width=Math.min(100,Math.max(0,Number(d.vuLeft)||0)*100/255)+'%';$('radioPlayerVuR').style.width=Math.min(100,Math.max(0,Number(d.vuRight)||0)*100/255)+'%';$('radioVol').value=Number(d.volume)||0;$('radioVolV').textContent=(Number(d.volume)||0)+'%';setRadioArtwork(d.artwork||(match&&match.artwork)||'');$('radioNow').textContent=d.error?d.error:(d.active?'On air':'WebRadio idle')}catch(e){$('radioNow').textContent='Status timeout: '+e}finally{radioStatusInFlight=false}}

async function updateOnce(){try{const s=await fetchJson('/api/status',{cache:'no-store'},1800);$('title').textContent=s.title||'No track';$('path').textContent=s.path||'';if(Number(s.artworkGeneration)!==artworkGeneration){artworkGeneration=Number(s.artworkGeneration)||0;const img=$('artwork'),fb=$('artFallback');if(s.artworkReady&&s.artworkUrl){img.onload=()=>{img.style.display='block';fb.style.display='none'};img.onerror=()=>{img.style.display='none';fb.style.display='block'};img.src=s.artworkUrl+'&t='+Date.now()}else{img.removeAttribute('src');img.style.display='none';fb.style.display='block'}}const codec={1:'WAV',2:'MP3',3:'FLAC'}[s.codec]||'NONE';$('meta').innerHTML=[codec,s.sampleRate?((s.sampleRate/1000).toFixed(1)+' kHz'):'',s.bits?(s.bits+' bit'):'',s.channels===2?'Stereo':s.channels===1?'Mono':'',s.state].filter(Boolean).map(x=>`<span class="pill">${esc(x)}</span>`).join('');$('bar').style.width=s.progress+'%';$('elapsed').textContent=fmt(s.elapsed);$('duration').textContent=fmt(s.duration);$('vol').value=s.volume;$('volv').textContent=s.volume+'%';$('settingsVol').value=s.volume;$('settingsVolV').textContent=s.volume+'%';$('brightness').value=s.displayBrightness||255;$('brightnessV').textContent=s.displayBrightness||255;$('segments').value=s.radioVuSegments||24;$('segv').textContent=s.radioVuSegments||24;$('vufps').value=s.radioVuFps||20;$('fpsv').textContent=s.radioVuFps||20;$('vuhold').value=s.radioVuHold||450;$('holdv').textContent=(s.radioVuHold||450)+' ms';$('vudecay').value=s.radioVuDecay||5;$('decayv').textContent=s.radioVuDecay||5;const apiVuStyle=['line','thin','rect','dot','neon','center'][Number(s.radioVuStyle)]||'rect';if(vuStylePending&&apiVuStyle===vuStylePending){vuStylePending='';vuStylePendingUntil=0}if(!vuStyleEditing&&(!vuStylePending||Date.now()>vuStylePendingUntil)){$('vustyle').value=apiVuStyle;if(Date.now()>vuStylePendingUntil)vuStylePending=''};$('vugain').value=s.radioVuGain||100;$('gainv').textContent=(s.radioVuGain||100)+'%';$('vugate').value=s.radioVuGate||2;$('gatev').textContent=s.radioVuGate||2;if(!eqEditing){const apiPreset=(s.eqPreset||'flat');const confirmed=!eqPending||(eqPending.preset===apiPreset&&(eqPending.bass===undefined||Number(s.eqBass)===eqPending.bass)&&(eqPending.mid===undefined||Number(s.eqMid)===eqPending.mid)&&(eqPending.treble===undefined||Number(s.eqTreble)===eqPending.treble));if(confirmed||Date.now()>eqPendingUntil){eqPending=null;$('eqPreset').value=apiPreset;$('eqBass').value=Number(s.eqBass)||0;$('eqMid').value=Number(s.eqMid)||0;$('eqTreble').value=Number(s.eqTreble)||0;$('eqBassV').textContent=(Number(s.eqBass)||0)+' dB';$('eqMidV').textContent=(Number(s.eqMid)||0)+' dB';$('eqTrebleV').textContent=(Number(s.eqTreble)||0)+' dB'}else if(eqPending){if(eqPending.preset)$('eqPreset').value=eqPending.preset;if(eqPending.bass!==undefined){$('eqBass').value=eqPending.bass;$('eqBassV').textContent=eqPending.bass+' dB'}if(eqPending.mid!==undefined){$('eqMid').value=eqPending.mid;$('eqMidV').textContent=eqPending.mid+' dB'}if(eqPending.treble!==undefined){$('eqTreble').value=eqPending.treble;$('eqTrebleV').textContent=eqPending.treble+' dB'}}}$('audioMetrics').innerHTML=metric('State',s.state,s.playing?'ok':'')+metric('Health',s.health+'%',s.health===100?'ok':'warn')+metric('Underruns',s.underruns,s.underruns?'warn':'ok')+metric('Short writes',s.shortWrites,s.shortWrites?'warn':'ok');$('systemMetrics').innerHTML=metric('Heap',s.heap)+metric('PSRAM',s.psram)+metric('Core0 loops',s.core0Loops)+metric('Core1 loops',s.core1Loops)+metric('Web gap max/10s',(s.webTickGapMaxMs||0)+' ms',(s.webTickGapMaxMs||0)>500?'warn':'ok')+metric('Queue drops',s.queueDrops,s.queueDrops?'warn':'ok')+metric('Radio stack',s.radioStackHighWater||0);$('transportMetrics').innerHTML=metric('SD',s.sd?'OK':'FAIL',s.sd?'ok':'warn')+metric('SD clock',(s.sdClockMHz||0)+' MHz')+metric('Tracks',s.tracks)+metric('Position',(s.playlistIndex||0)+' / '+(s.playlistCount||0))+metric('Web queue',s.commandQueue+'/8')+metric('Radio commands',(s.radioCommandsQueued||0)+' / drop '+(s.radioCommandsDropped||0))+metric('Requests',s.requests);const ci=Math.max(-1,Number(s.playlistIndex||0)-1);if(ci!==nowCurrent){nowCurrent=ci;if(!nowQuery&&(ci<nowOffset||ci>=nowOffset+nowLimit))nowOffset=Math.floor(Math.max(0,ci)/nowLimit)*nowLimit;loadNowPlaylist(false)}}catch(e){$('title').textContent='Offline';$('path').textContent=String(e)}}
let updateInFlight=false;async function update(){if(updateInFlight)return;updateInFlight=true;try{await updateOnce()}finally{updateInFlight=false}}
async function wifiStatus(){try{const w=await(await fetch('/api/wifi/status',{cache:'no-store'})).json();$('wifiMetrics').innerHTML=metric('Mode',w.mode,w.ap?'ok':'')+metric('Status',w.status,w.connected?'ok':'warn')+metric('AP SSID',w.apSsid)+metric('AP address',w.apIp,w.ap?'ok':'')+metric('STA SSID',w.ssid||'—')+metric('STA address',w.ip,w.connected?'ok':'')+metric('RSSI',w.connected?(w.rssi+' dBm'):'—')+metric('Saved profile',w.savedSsid||'—')+metric('AutoConnect',w.autoConnect?'ON':'OFF',w.autoConnect?'ok':'warn')+metric('Fallback AP',w.fallbackAp?'ON':'OFF',w.fallbackAp?'ok':'warn')+metric('Reconnects',w.reconnects)}catch(e){$('wifiToast').textContent='WiFi status error: '+e}}
async function wifiScan(start=true){$('wifiToast').textContent='Scanning…';try{let r=await(await fetch('/api/wifi/scan'+(start?'?start=1':''),{cache:'no-store'})).json();if(r.running){setTimeout(()=>wifiScan(false),700);return}const box=$('wifiNetworks'),sel=$('wifiSsid');sel.innerHTML='<option value="">Select network</option>';box.innerHTML='';for(const n of r.networks||[]){const o=document.createElement('option');o.value=n.ssid;o.textContent=n.ssid+' ('+n.rssi+' dBm)';sel.appendChild(o);const d=document.createElement('div');d.className='metric';d.innerHTML='<span>'+esc(n.ssid||'[hidden]')+'</span><span>'+n.rssi+' dBm · ch '+n.channel+(n.open?' · open':' · secured')+'</span>';box.appendChild(d)}$('wifiToast').textContent=(r.networks||[]).length+' networks found'}catch(e){$('wifiToast').textContent='Scan error: '+e}}
async function wifiPost(path,data={}){const body=new URLSearchParams(data);const r=await fetch(path,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});const j=await r.json();$('wifiToast').textContent=j.ok?'Command accepted':(j.error||'Error');setTimeout(wifiStatus,700)}

function saveNowResume(){try{const box=$('nowTrackList');localStorage.setItem('iq200.web.nowplaylist.v1',JSON.stringify({offset:nowOffset,q:nowQuery,scrollTop:box?box.scrollTop:0,current:nowCurrent,ts:Date.now()}))}catch(e){}}
function loadNowResume(){try{const x=JSON.parse(localStorage.getItem('iq200.web.nowplaylist.v1')||'{}');nowOffset=Math.max(0,Number(x.offset)||0);nowQuery=String(x.q||'');if($('nowTrackSearch'))$('nowTrackSearch').value=nowQuery;setTimeout(()=>{if($('nowTrackList'))$('nowTrackList').scrollTop=Math.max(0,Number(x.scrollTop)||0)},50)}catch(e){}}
function nowRowHtml(t){return `<div class="nowtrack ${t.current?'current':''}" data-now-index="${t.index}"><div class="tracknum">${t.current?'<span class="playmark">▶</span>':t.number}</div><div><div class="tracktitle">${esc(t.title)}</div><div class="trackpath">${esc(t.codec)} · ${esc(t.path)}</div></div><button data-now-play-index="${t.index}">Play</button></div>`}
async function loadNowPlaylist(center=false){
  const box=$('nowTrackList');if(!box)return;
  nowQuery=($('nowTrackSearch')?.value||'').trim();
  if(center&&!nowQuery&&nowCurrent>=0)nowOffset=Math.floor(nowCurrent/nowLimit)*nowLimit;
  $('nowTrackInfo').textContent='Loading playlist…';
  try{const u='/api/library?offset='+nowOffset+'&limit='+nowLimit+'&q='+encodeURIComponent(nowQuery);const d=await(await fetch(u,{cache:'no-store'})).json();
    const rows=(d.items||[]).map(nowRowHtml).join('');box.innerHTML=rows||'<div class="now-empty">No tracks found</div>';
    $('nowTrackInfo').textContent=(d.matched??d.total??0)+' tracks';$('nowTrackPage').textContent='Showing '+(rows?((d.offset||0)+1):0)+'–'+Math.min((d.offset||0)+(d.items||[]).length,d.matched??d.total??0)+' of '+(d.matched??d.total??0);$('nowTrackPosition').textContent='Playing '+((d.current??-1)+1)+' / '+(d.total||0);
    nowLoadedCurrent=d.current??-1;requestAnimationFrame(()=>scrollNowCurrent(false));saveNowResume();
  }catch(e){$('nowTrackInfo').textContent='Playlist error: '+e}
}
function scrollNowCurrent(force){const box=$('nowTrackList');if(!box||nowUserScrolling)return;const row=box.querySelector('.nowtrack.current');if(!row)return;const top=row.offsetTop,bottom=top+row.offsetHeight;if(force||top<box.scrollTop||bottom>box.scrollTop+box.clientHeight)row.scrollIntoView({block:'center',behavior:force?'smooth':'auto'})}
async function playNowTrack(index){saveNowResume();try{const body=new URLSearchParams({index:String(index)});const r=await fetch('/api/library/play',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});const j=await r.json();$('nowTrackInfo').textContent=j.ok?'Track queued':'Play rejected: '+(j.error||'unknown');if(j.ok){nowCurrent=index;setTimeout(()=>{loadNowPlaylist(false);update()},500)}}catch(e){$('nowTrackInfo').textContent='Play error: '+e}}
async function loadTracks(reset=false){
  if(reset)trackOffset=0;
  trackQuery=($('trackSearch')?.value||'').trim();
  $('trackInfo').textContent='Loading tracks…';
  try{
    const u='/api/library?offset='+trackOffset+'&limit='+trackLimit+'&q='+encodeURIComponent(trackQuery);
    const d=await(await fetch(u,{cache:'no-store'})).json();
    trackMatched=d.matched??d.total??0;
    const rows=(d.items||[]).map(t=>`<div class="track ${t.current?'current':''}"><div class="tracknum">${t.number}</div><div><div class="tracktitle">${esc(t.title)}</div><div class="trackpath">${esc(t.codec)} · ${esc(t.path)}</div></div><button data-play-index="${t.index}">▶ Play</button></div>`).join('');
    $('trackList').innerHTML=rows||'<div class="path">No tracks found. Run scan/dbload first.</div>';
    const page=Math.floor(trackOffset/trackLimit)+1,totalPages=Math.max(1,Math.ceil(trackMatched/trackLimit));
    $('trackPage').textContent='Page '+page+' / '+totalPages;
    $('trackPrevPage').disabled=trackOffset<=0;
    $('trackNextPage').disabled=trackOffset+trackLimit>=trackMatched;
    $('trackInfo').textContent=trackMatched+' matching tracks · active playlist '+(d.total||0);
    saveTrackResume();
    restoreTrackScroll();
  }catch(e){$('trackInfo').textContent='Library error: '+e}
}
async function playTrack(index){
  saveTrackResume();
  trackRestoreScroll=window.scrollY||0;
  try{const body=new URLSearchParams({index:String(index)});const r=await fetch('/api/library/play',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});const j=await r.json();$('trackInfo').textContent=j.ok?'Track queued · list position saved':'Play rejected: '+(j.error||'unknown');if(j.ok){document.querySelectorAll('.tab,.view').forEach(x=>x.classList.remove('active'));document.querySelector('[data-view="library"]')?.classList.add('active');$('library').classList.add('active')}setTimeout(()=>{loadTracks(false);update()},700)}catch(e){$('trackInfo').textContent='Play error: '+e}
}
function render(q=''){q=q.toLowerCase();let out='',cat='';for(const e of all){const hay=(e.category+' '+e.command+' '+e.aliases+' '+e.description).toLowerCase();if(q&&!hay.includes(q))continue;if(e.category!==cat){cat=e.category;out+=`<h3 class="cat">${esc(cat)}</h3>`}out+=`<div class="command"><div><code>${esc(e.command)}</code><div class="alias">${e.aliases?('Aliases: '+esc(e.aliases)):''}</div></div><div class="desc">${esc(e.description)}</div><button data-template="${esc(e.command)}">Use</button></div>`}$('commands').innerHTML=out||'No commands found'}
document.addEventListener('click',e=>{const nr=e.target.closest('.nowtrack');if(nr&&!e.target.closest('button')&&nr.dataset.nowIndex!==undefined){playNowTrack(Number(nr.dataset.nowIndex));return}const b=e.target.closest('button');if(!b)return;if(b.dataset.view){saveTrackResume();showView(b.dataset.view);if(b.dataset.view==='library'){trackRestoreScroll=Number((JSON.parse(localStorage.getItem(TRACK_RESUME_KEY)||'{}')).scrollY)||0;loadTracks(false)}}if(b.dataset.mode)switchMode(b.dataset.mode);if(b.dataset.cmd)run(b.dataset.cmd);if(b.dataset.playIndex!==undefined)playTrack(Number(b.dataset.playIndex));if(b.dataset.nowPlayIndex!==undefined)playNowTrack(Number(b.dataset.nowPlayIndex));if(b.dataset.template!==undefined){$('cmd').value=b.dataset.template;$('cmd').focus()}});
$('send').onclick=()=>run($('cmd').value);$('cmd').onkeydown=e=>{if(e.key==='Enter')run(e.target.value)};$('search').oninput=e=>render(e.target.value);let vt;$('vol').oninput=e=>{$('volv').textContent=e.target.value+'%';clearTimeout(vt);vt=setTimeout(()=>run('volume '+e.target.value),140)};let svt;$('settingsVol').oninput=e=>{$('settingsVolV').textContent=e.target.value+'%';clearTimeout(svt);svt=setTimeout(()=>run('volume '+e.target.value),140)};let bdt;$('brightness').oninput=e=>{$('brightnessV').textContent=e.target.value;clearTimeout(bdt);bdt=setTimeout(()=>run('brightness '+e.target.value),140)};$('segments').oninput=e=>$('segv').textContent=e.target.value;$('segments').onchange=e=>run('vuseg '+e.target.value);$('vufps').oninput=e=>$('fpsv').textContent=e.target.value;$('vufps').onchange=e=>run('vu fps '+e.target.value);$('vuhold').oninput=e=>$('holdv').textContent=e.target.value+' ms';$('vuhold').onchange=e=>run('vu hold '+e.target.value);$('vudecay').oninput=e=>$('decayv').textContent=e.target.value;$('vudecay').onchange=e=>run('vu decay '+e.target.value);$('vugain').oninput=e=>$('gainv').textContent=e.target.value+'%';$('vugain').onchange=e=>run('vu gain '+e.target.value);$('vugate').oninput=e=>$('gatev').textContent=e.target.value;$('vugate').onchange=e=>run('vu gate '+e.target.value);$('applyPlayerLayout').onclick=()=>run('player theme '+$('playerLayout').value);const applyVuStyle=()=>{const style=$('vustyle').value;vuStylePending=style;vuStylePendingUntil=Date.now()+3000;run('vu style '+style)};$('vustyle').onfocus=()=>vuStyleEditing=true;$('vustyle').onblur=()=>{vuStyleEditing=false};$('vustyle').onchange=()=>{vuStyleEditing=false;applyVuStyle()};$('applyStyle').onclick=applyVuStyle;$('applyNav').onclick=()=>run('nav delay '+$('navdelay').value);for(const id of ['eqPreset','eqBass','eqMid','eqTreble']){$(id).onfocus=()=>eqEditing=true;$(id).onblur=()=>{setTimeout(()=>eqEditing=false,150)}}$('eqBass').oninput=e=>{$('eqBassV').textContent=e.target.value+' dB';eqEditing=true};$('eqMid').oninput=e=>{$('eqMidV').textContent=e.target.value+' dB';eqEditing=true};$('eqTreble').oninput=e=>{$('eqTrebleV').textContent=e.target.value+' dB';eqEditing=true};$('applyEqPreset').onclick=()=>run('eq '+$('eqPreset').value);$('applyEqCustom').onclick=()=>run('eq custom '+$('eqBass').value+' '+$('eqMid').value+' '+$('eqTreble').value);$('eqStatus').onclick=()=>run('eq');$('wifiRefresh').onclick=wifiStatus;$('wifiScan').onclick=()=>wifiScan(true);$('wifiReconnect').onclick=()=>wifiPost('/api/wifi/reconnect');$('wifiDisconnect').onclick=()=>wifiPost('/api/wifi/disconnect');$('wifiAutoOn').onclick=()=>wifiPost('/api/wifi/auto',{enabled:'1'});$('wifiAutoOff').onclick=()=>wifiPost('/api/wifi/auto',{enabled:'0'});$('wifiFallbackOn').onclick=()=>wifiPost('/api/wifi/fallback',{enabled:'1'});$('wifiFallbackOff').onclick=()=>wifiPost('/api/wifi/fallback',{enabled:'0'});$('wifiForget').onclick=()=>wifiPost('/api/wifi/forget');$('wifiSsid').onchange=e=>$('wifiSsidManual').value=e.target.value;$('wifiConnect').onclick=()=>{const ssid=$('wifiSsidManual').value.trim()||$('wifiSsid').value;wifiPost('/api/wifi/connect',{ssid,password:$('wifiPassword').value,keepAp:'1'})};$('wifiSave').onclick=()=>{const ssid=$('wifiSsidManual').value.trim()||$('wifiSsid').value;wifiPost('/api/wifi/save',{ssid,password:$('wifiPassword').value})};$('trackRefresh').onclick=()=>loadTracks(true);$('trackPrevPage').onclick=()=>{trackOffset=Math.max(0,trackOffset-trackLimit);trackRestoreScroll=0;saveTrackResume();loadTracks(false)};$('trackNextPage').onclick=()=>{trackOffset+=trackLimit;trackRestoreScroll=0;saveTrackResume();loadTracks(false)};let trackTimer;$('trackSearch').oninput=()=>{clearTimeout(trackTimer);trackTimer=setTimeout(()=>{trackRestoreScroll=0;loadTracks(true)},250)};$('nowTrackRefresh').onclick=()=>loadNowPlaylist(true);let nowSearchTimer;$('nowTrackSearch').oninput=()=>{clearTimeout(nowSearchTimer);nowSearchTimer=setTimeout(()=>{nowOffset=0;loadNowPlaylist(false)},250)};$('nowTrackList').addEventListener('scroll',()=>{nowUserScrolling=true;clearTimeout(nowScrollTimer);nowScrollTimer=setTimeout(()=>{nowUserScrolling=false;saveNowResume()},700)});$('radioRefresh').onclick=loadRadioStations;$('radioSearch').oninput=renderRadioStations;$('radioSave').onclick=saveRadio;$('radioNew').onclick=clearRadioEditor;$('radioDelete').onclick=deleteRadio;$('radioImportAdd').onclick=()=>importRadio(false);$('radioImportReplace').onclick=()=>importRadio(true);$('radioExportJson').onclick=()=>exportRadio('json');$('radioExportM3u').onclick=()=>exportRadio('m3u');$('radioStop').onclick=stopRadio;$('radioStatusBtn').onclick=updateRadioStatus;window.addEventListener('beforeunload',()=>{saveTrackResume();saveNowResume()});
let rvt;$('radioVol').oninput=e=>{$('radioVolV').textContent=e.target.value+'%';clearTimeout(rvt);rvt=setTimeout(()=>run('volume '+e.target.value),140)};$('radioPrev').onclick=()=>stepRadio(-1);$('radioPlay').onclick=()=>playRadio(radioCurrentId>=0?radioCurrentId:(radioCache.length?Number(radioCache[0].id):-1));$('radioNext').onclick=()=>stepRadio(1);
(async()=>{try{all=(await(await fetch('/api/commands')).json()).commands;render()}catch(e){$('commands').textContent='Offline'}})();$('artwork').addEventListener('error',()=>{artworkGeneration=-1;$('artwork').removeAttribute('src')});setInterval(()=>{if(!$('artwork').hasAttribute('src'))artworkGeneration=-1},3000);setInterval(update,750);setInterval(modeStatus,3000);setInterval(()=>{if($('webradio').classList.contains('active'))updateRadioStatus()},500);modeStatus();update();wifiStatus();
</script></body></html>)HTML";
      server.sendHeader("Cache-Control", "no-store");
      server.send(200, "text/html; charset=utf-8", FPSTR(PAGE));
    });

    server.on("/api/artwork", HTTP_GET, [this]() {
      if (!rt || rt->systemMode != IQ200_MODE_LOCAL_PLAYER) {
        sendJson(409, R"({"ok":false,"error":"local_player_mode_required"})");
        return;
      }
      ArtworkCache& art = ArtworkCache::instance();
      ArtworkCache::ReadGuard guard(art, pdMS_TO_TICKS(5));
      if (!guard.locked() || !art.data() || !art.size() ||
          (art.state() != ArtworkCache::JPEG_READY && art.state() != ArtworkCache::PNG_READY)) {
        server.sendHeader("Cache-Control", "no-store");
        server.send(503, "application/json", R"({"ok":false,"error":"artwork_busy"})");
        return;
      }
      const size_t artBytes = art.size();
      if (artBytes > WEB_ARTWORK_MAX_BYTES) {
        server.sendHeader("Cache-Control", "no-store");
        server.send(413, "application/json", R"({"ok":false,"error":"artwork_too_large_for_live_web"})");
        return;
      }
      const char* mime = art.state() == ArtworkCache::PNG_READY ? "image/png" : "image/jpeg";
      WiFiClient client = server.client();
      client.setTimeout(150);
      client.printf("HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %u\r\nCache-Control: public, max-age=31536000, immutable\r\nConnection: close\r\n\r\n", mime, (unsigned)artBytes);
      const uint8_t* data = art.data();
      size_t sent = 0;
      const uint32_t sendStarted = millis();
      static constexpr size_t kWebChunk = 2048;
      while (sent < artBytes && client.connected() &&
             (uint32_t)(millis() - sendStarted) < WEB_ARTWORK_SEND_BUDGET_MS) {
        const size_t ask = min(kWebChunk, artBytes - sent);
        const size_t n = client.write(data + sent, ask);
        if (!n) break;
        sent += n;
        delay(1);
      }
      if (sent != artBytes) {
        Serial.printf("[WEB][ART] bounded transfer stopped sent=%u/%u elapsed=%lums\n",
                      (unsigned)sent, (unsigned)artBytes,
                      (unsigned long)(millis() - sendStarted));
      }
      client.stop();
    });
    server.on("/api/status", HTTP_GET, [this]() { sendJson(200, statusJson()); });
    server.on("/api/player", HTTP_GET, [this]() { sendJson(200, statusJson()); });
    server.on("/api/diagnostics", HTTP_GET, [this]() { sendJson(200, statusJson()); });
    server.on("/api/mode", HTTP_GET, [this]() { sendJson(200, modeStatusJson()); });
    server.on("/api/mode/switch", HTTP_POST, [this]() {
      if (!rt || !server.hasArg("mode")) {
        sendJson(400, R"({"ok":false,"error":"missing_mode"})");
        return;
      }
      const IQ200Mode requested = ModeManager::parse(server.arg("mode"));
      if (static_cast<uint8_t>(requested) == 255U) {
        sendJson(400, R"({"ok":false,"error":"invalid_mode"})");
        return;
      }
      if (!ModeManager::available(requested)) {
        sendJson(409, R"({"ok":false,"error":"mode_reserved_for_future_hardware"})");
        return;
      }
      if (rt->systemMode == static_cast<uint8_t>(requested)) {
        sendJson(200, String("{\"ok\":true,\"alreadyActive\":true,\"current\":\"") + ModeManager::name(requested) + "\"}");
        return;
      }
      rt->modeSwitchRequest = static_cast<int8_t>(requested);
      sendJson(202, String("{\"ok\":true,\"rebootingTo\":\"") + ModeManager::name(requested) + "\"}");
    });
    server.on("/api/library", HTTP_GET, [this]() {
      if (!rt || rt->systemMode != IQ200_MODE_LOCAL_PLAYER) { sendJson(409, R"({"ok":false,"error":"local_player_mode_required"})"); return; }
      const int offset = server.hasArg("offset") ? server.arg("offset").toInt() : 0;
      const int limit = server.hasArg("limit") ? server.arg("limit").toInt() : 50;
      const String q = server.hasArg("q") ? server.arg("q") : String("");
      sendJson(200, libraryJson(offset, limit, q));
    });
    server.on("/api/library/play", HTTP_POST, [this]() {
      if (!rt || rt->systemMode != IQ200_MODE_LOCAL_PLAYER) { sendJson(409, R"({"ok":false,"error":"local_player_mode_required"})"); return; }
      if (!rt || !playlist || !server.hasArg("index")) { sendJson(400, R"({"ok":false,"error":"missing_index"})"); return; }
      const int idx = server.arg("index").toInt();
      if (idx < 0 || idx >= playlist->size()) { sendJson(400, R"({"ok":false,"error":"index_out_of_range"})"); return; }
      rt->webTrackPlayIndex = idx;
      rt->webTrackPlayRequest = true;
      sendJson(202, R"({"ok":true})");
    });
    server.on("/api/radio/status", HTTP_GET, [this]() {
      RadioService::Snapshot s;
      if (radioPlayback) radioPlayback->snapshot(s);
      String j = "{\"ok\":true,\"state\":\"" + jsonEscape(rt ? rt->radioStatus : "IDLE") +
                 "\",\"station\":\"" + jsonEscape(s.station) +
                 "\",\"title\":\"" + jsonEscape(s.title) +
                 "\",\"artwork\":\"" + jsonEscape(s.artwork) +
                 "\",\"error\":\"" + jsonEscape(s.error) +
                 "\",\"active\":" + String(s.active ? "true" : "false") +
                 ",\"volume\":" + String(rt ? rt->volumePercent : 0) +
                 ",\"vuLeft\":" + String(rt ? rt->radioVuLeft : 0) +
                 ",\"vuRight\":" + String(rt ? rt->radioVuRight : 0) +
                 ",\"queued\":" + String(s.commandsQueued) +
                 ",\"dropped\":" + String(s.commandsDropped) +
                 ",\"unsupported\":" + String(s.unsupportedStreams) + "}";
      sendJson(200, j);
    });
    server.on("/api/radio/play", HTTP_POST, [this]() {
      if (!rt || rt->systemMode != IQ200_MODE_WEBRADIO) { sendJson(409, R"({"ok":false,"error":"webradio_mode_required"})"); return; }
      if (!radioPlayback || !server.hasArg("id")) { sendJson(503, R"({"ok":false,"error":"radio_unavailable"})"); return; }
      const int id = server.arg("id").toInt();
      const RadioStation* st = (id >= 0 && id < radioStations.count()) ? radioStations.get((uint8_t)id) : nullptr;
      if (!st) { sendJson(400, R"({"ok":false,"error":"invalid_id"})"); return; }
      const bool ok = radioPlayback->play(st->name, st->url, st->artwork);
      sendJson(ok ? 202 : 503, ok ? R"({"ok":true,"queued":true})" : R"({"ok":false,"error":"radio_command_rejected"})");
    });
    server.on("/api/radio/stop", HTTP_POST, [this]() {
      if (!rt || rt->systemMode != IQ200_MODE_WEBRADIO) { sendJson(409, R"({"ok":false,"error":"webradio_mode_required"})"); return; }
      if (!radioPlayback) { sendJson(503, R"({"ok":false,"error":"radio_unavailable"})"); return; }
      const bool ok = radioPlayback->stop();
      sendJson(ok ? 202 : 503, ok ? R"({"ok":true,"queued":true})" : R"({"ok":false,"error":"radio_command_rejected"})");
    });
    server.on("/api/radio/stations", HTTP_GET, [this]() { sendJson(200, radioStationsJson()); });
    server.on("/api/radio/station", HTTP_POST, [this]() {
      RadioStation st; st.name=server.arg("name"); st.url=server.arg("url"); st.genre=server.arg("genre"); st.country=server.arg("country"); st.artwork=server.arg("artwork"); st.favorite=server.arg("favorite")=="1";
      const int id=server.hasArg("id")?server.arg("id").toInt():-1;
      const bool ok=radioStations.upsert(id,st); sendJson(ok?200:400,ok?R"({"ok":true})":String("{\"ok\":false,\"error\":\"")+jsonEscape(radioStations.lastError().c_str())+"\"}");
    });
    server.on("/api/radio/station/delete", HTTP_POST, [this]() {
      const int id=server.hasArg("id")?server.arg("id").toInt():-1; const bool ok=id>=0&&radioStations.removeAt((uint8_t)id);
      sendJson(ok?200:400,ok?R"({"ok":true})":R"({"ok":false,"error":"invalid_id"})");
    });
    server.on("/api/radio/import", HTTP_POST, [this]() {
      if(!server.hasArg("text")){sendJson(400,R"({"ok":false,"error":"missing_text"})");return;}
      const uint8_t added=radioStations.importText(server.arg("text"),server.arg("replace")=="1");
      sendJson(200,String("{\"ok\":true,\"added\":")+String(added)+",\"count\":"+String(radioStations.count())+"}");
    });
    server.on("/api/radio/export", HTTP_GET, [this]() {
      requests++; const String fmt=server.hasArg("format")?server.arg("format"):"json";
      server.sendHeader("Content-Disposition",fmt=="m3u"?"attachment; filename=iq200_stations.m3u":"attachment; filename=iq200_stations.json");
      server.send(200,fmt=="m3u"?"audio/x-mpegurl; charset=utf-8":"application/json; charset=utf-8",fmt=="m3u"?radioExportM3u():radioExportJson());
    });
    server.on("/api/wifi/status", HTTP_GET, [this]() { sendJson(200, wifiStatusJson()); });
    server.on("/api/wifi/scan", HTTP_GET, [this]() { sendJson(200, wifiScanJson(server.hasArg("start"))); });
    server.on("/api/wifi/connect", HTTP_POST, [this]() {
      if (!rt || !server.hasArg("ssid") || !server.arg("ssid").length()) {
        sendJson(400, R"({"ok":false,"error":"missing_ssid"})");
        return;
      }
      const String ssid = server.arg("ssid");
      const String password = server.hasArg("password") ? server.arg("password") : String("");
      strncpy(rt->wifiPendingSsid, ssid.c_str(), sizeof(rt->wifiPendingSsid) - 1);
      rt->wifiPendingSsid[sizeof(rt->wifiPendingSsid) - 1] = 0;
      strncpy(rt->wifiPendingPassword, password.c_str(), sizeof(rt->wifiPendingPassword) - 1);
      rt->wifiPendingPassword[sizeof(rt->wifiPendingPassword) - 1] = 0;
      if (server.hasArg("keepAp") && server.arg("keepAp") == "1") rt->wifiApStaConnectRequest = true;
      else rt->wifiStaConnectRequest = true;
      sendJson(202, R"({"ok":true})");
    });
    server.on("/api/wifi/save", HTTP_POST, [this]() {
      if (!rt || !server.hasArg("ssid") || !server.arg("ssid").length()) {
        sendJson(400, R"({"ok":false,"error":"missing_ssid"})");
        return;
      }
      const String ssid = server.arg("ssid");
      const String password = server.hasArg("password") ? server.arg("password") : String("");
      strncpy(rt->wifiPendingSsid, ssid.c_str(), sizeof(rt->wifiPendingSsid) - 1);
      rt->wifiPendingSsid[sizeof(rt->wifiPendingSsid) - 1] = 0;
      strncpy(rt->wifiPendingPassword, password.c_str(), sizeof(rt->wifiPendingPassword) - 1);
      rt->wifiPendingPassword[sizeof(rt->wifiPendingPassword) - 1] = 0;
      rt->wifiSaveRequest = true;
      sendJson(202, R"({"ok":true})");
    });
    server.on("/api/wifi/disconnect", HTTP_POST, [this]() {
      if (!rt) { sendJson(503, R"({"ok":false,"error":"not_ready"})"); return; }
      rt->wifiDisconnectRequest = true;
      sendJson(202, R"({"ok":true})");
    });
    server.on("/api/wifi/reconnect", HTTP_POST, [this]() {
      if (!rt) { sendJson(503, R"({"ok":false,"error":"not_ready"})"); return; }
      rt->wifiLoadRequest = true;
      sendJson(202, R"({"ok":true})");
    });
    server.on("/api/wifi/auto", HTTP_POST, [this]() {
      if (!rt || !server.hasArg("enabled")) { sendJson(400, R"({"ok":false,"error":"missing_enabled"})"); return; }
      if (server.arg("enabled") == "1") rt->wifiAutoOnRequest = true; else rt->wifiAutoOffRequest = true;
      sendJson(202, R"({"ok":true})");
    });
    server.on("/api/wifi/fallback", HTTP_POST, [this]() {
      if (!rt || !server.hasArg("enabled")) { sendJson(400, R"({"ok":false,"error":"missing_enabled"})"); return; }
      if (server.arg("enabled") == "1") rt->wifiFallbackOnRequest = true; else rt->wifiFallbackOffRequest = true;
      sendJson(202, R"({"ok":true})");
    });
    server.on("/api/wifi/forget", HTTP_POST, [this]() {
      if (!rt) { sendJson(503, R"({"ok":false,"error":"not_ready"})"); return; }
      rt->wifiForgetRequest = true;
      sendJson(202, R"({"ok":true})");
    });
    server.on("/api/player/action", HTTP_POST, [this]() {
      if (!rt || rt->systemMode != IQ200_MODE_LOCAL_PLAYER) { sendJson(409, R"({"ok":false,"error":"local_player_mode_required"})"); return; }
      String action = server.hasArg("action") ? server.arg("action") : "";
      if (action != "play" && action != "stop" && action != "next" && action != "prev") {
        sendJson(400, R"({"ok":false,"error":"invalid_action"})"); return;
      }
      bool ok = enqueueCommand(action);
      sendJson(ok ? 202 : 429, ok ? R"({"ok":true})" : R"({"ok":false,"error":"queue_full"})");
    });
    server.on("/api/commands", HTTP_GET, [this]() { sendJson(200, commandsJson()); });
    server.on("/api/command/status", HTTP_GET, [this]() { sendJson(200, statusJson()); });
    server.on("/api/command", HTTP_POST, [this]() {
      String c;
      if (server.hasArg("cmd")) c = server.arg("cmd");
      else if (server.hasArg("plain")) c = server.arg("plain");
      if (!c.length()) { sendJson(400, "{\"ok\":false,\"error\":\"missing_cmd\"}"); return; }
      if (!enqueueCommand(c)) { sendJson(429, "{\"ok\":false,\"error\":\"queue_full_or_invalid\"}"); return; }
      String j = "{\"ok\":true,\"queued\":\"" + jsonEscape(c.c_str()) + "\",\"depth\":" + String(commandCount) + "}";
      sendJson(202, j);
    });

    // Backward-compatible endpoints now use the exact same command path.
    server.on("/api/play", HTTP_POST, [this]() { if (!rt || rt->systemMode != IQ200_MODE_LOCAL_PLAYER) { sendJson(409, R"({"ok":false,"error":"local_player_mode_required"})"); return; } enqueueCommand("play"); sendJson(202, "{\"ok\":true,\"cmd\":\"play\"}"); });
    server.on("/api/stop", HTTP_POST, [this]() { if (!rt || rt->systemMode != IQ200_MODE_LOCAL_PLAYER) { sendJson(409, R"({"ok":false,"error":"local_player_mode_required"})"); return; } enqueueCommand("stop"); sendJson(202, "{\"ok\":true,\"cmd\":\"stop\"}"); });
    server.on("/api/next", HTTP_POST, [this]() { if (!rt || rt->systemMode != IQ200_MODE_LOCAL_PLAYER) { sendJson(409, R"({"ok":false,"error":"local_player_mode_required"})"); return; } enqueueCommand("next"); sendJson(202, "{\"ok\":true,\"cmd\":\"next\"}"); });
    server.on("/api/prev", HTTP_POST, [this]() { if (!rt || rt->systemMode != IQ200_MODE_LOCAL_PLAYER) { sendJson(409, R"({"ok":false,"error":"local_player_mode_required"})"); return; } enqueueCommand("prev"); sendJson(202, "{\"ok\":true,\"cmd\":\"prev\"}"); });
    server.on("/api/scan", HTTP_POST, [this]() { if (!rt || rt->systemMode != IQ200_MODE_LOCAL_PLAYER) { sendJson(409, R"({"ok":false,"error":"local_player_mode_required"})"); return; } enqueueCommand("scan"); sendJson(202, "{\"ok\":true,\"cmd\":\"scan\"}"); });
    server.on("/api/volume", HTTP_POST, [this]() {
      if (!server.hasArg("v")) { sendJson(400, "{\"ok\":false,\"error\":\"missing_v\"}"); return; }
      String c = "volume " + String(constrain(server.arg("v").toInt(), 0, 100));
      bool ok = enqueueCommand(c);
      sendJson(ok ? 202 : 429, ok ? "{\"ok\":true,\"cmd\":\"volume\"}" : "{\"ok\":false,\"error\":\"queue_full\"}");
    });
    // Captive portal probes used by Android, iOS, Windows and Firefox.
    auto portalRedirect = [this]() {
      const wifi_mode_t mode = WiFi.getMode();
      const bool apReady = (mode == WIFI_AP || mode == WIFI_AP_STA) && WiFi.softAPIP() != IPAddress(0,0,0,0);
      server.sendHeader("Cache-Control", "no-store");
      if (apReady) {
        server.sendHeader("Location", "http://192.168.4.1/", true);
        server.send(302, "text/plain", "IQ200 OS WiFi setup");
      } else {
        server.send(204, "text/plain", "");
      }
    };
    server.on("/generate_204", HTTP_ANY, portalRedirect);
    server.on("/gen_204", HTTP_ANY, portalRedirect);
    server.on("/hotspot-detect.html", HTTP_ANY, portalRedirect);
    server.on("/library/test/success.html", HTTP_ANY, portalRedirect);
    server.on("/ncsi.txt", HTTP_ANY, portalRedirect);
    server.on("/connecttest.txt", HTTP_ANY, portalRedirect);
    server.on("/redirect", HTTP_ANY, portalRedirect);
    server.on("/canonical.html", HTTP_ANY, portalRedirect);
    server.on("/success.txt", HTTP_ANY, portalRedirect);
    server.on("/fwlink", HTTP_ANY, portalRedirect);
    server.onNotFound([this]() {
      const wifi_mode_t mode = WiFi.getMode();
      const bool apReady = (mode == WIFI_AP || mode == WIFI_AP_STA) && WiFi.softAPIP() != IPAddress(0,0,0,0);
      if (apReady) {
        server.sendHeader("Location", "http://192.168.4.1/", true);
        server.sendHeader("Cache-Control", "no-store");
        server.send(302, "text/plain", "IQ200 OS WiFi setup");
      } else {
        sendJson(404, "{\"ok\":false,\"error\":\"not_found\"}");
      }
    });
  }

public:
  explicit WebServerService(RadioStationStore& stationStore) : radioStations(stationStore) {}

  void begin(RuntimeState& state, StorageService& st, CommandManager& commandRegistry, PlaylistManager& activePlaylist, RadioService& radioService) {
    rt = &state;
    storage = &st;
    commands = &commandRegistry;
    playlist = &activePlaylist;
    radioPlayback = &radioService;
    radioStations.begin();
    setupRoutes();
    rt->webEnabled = false;
    rt->webRunning = false;
    rt->webPort = 80;
    strncpy(rt->webStatus, "READY_OFF", sizeof(rt->webStatus) - 1);
    rt->webStatus[sizeof(rt->webStatus) - 1] = 0;
  }

  bool popCommand(String& out) {
    bool ok = false;
    char local[COMMAND_MAX_LEN + 1] = "";
    portENTER_CRITICAL(&commandMux);
    if (commandCount) {
      strncpy(local, commandQueue[commandHead], COMMAND_MAX_LEN);
      local[COMMAND_MAX_LEN] = 0;
      commandHead = (uint8_t)((commandHead + 1) % COMMAND_QUEUE_SIZE);
      commandCount--;
      strncpy(lastExecuted, local, COMMAND_MAX_LEN);
      lastExecuted[COMMAND_MAX_LEN] = 0;
      ok = true;
    }
    portEXIT_CRITICAL(&commandMux);
    if (ok) out = local;
    return ok;
  }

  void enable(bool on) {
    if (!rt) return;
    rt->webEnabled = on;
    if (!on && running) {
      running = false;
      rt->webRunning = false;
      strncpy(rt->webStatus, "STOPPED", sizeof(rt->webStatus) - 1);
      rt->webStatus[sizeof(rt->webStatus) - 1] = 0;
      Serial.println("[WEB] stopped");
    }
  }

  void tick() {
    if (!rt || !rt->webEnabled) {
      lastTickMs = 0;
      gapWindowStartedMs = 0;
      return;
    }
    const uint32_t now = millis();
    if (lastTickMs) {
      const uint32_t gap = now - lastTickMs;
      rt->webTickGapLastMs = gap;
      if (!gapWindowStartedMs || now - gapWindowStartedMs >= 10000U) {
        gapWindowStartedMs = now;
        rt->webTickGapMaxMs = gap;
      } else if (gap > rt->webTickGapMaxMs) {
        rt->webTickGapMaxMs = gap;
      }
    } else {
      gapWindowStartedMs = now;
      rt->webTickGapLastMs = 0;
      rt->webTickGapMaxMs = 0;
    }
    lastTickMs = now;
    rt->webTicks++;
    if (rt->scanLock) return;
    if (WiFi.getMode() == WIFI_OFF) return;
    const wifi_mode_t mode = WiFi.getMode();
    const bool apReady = (mode == WIFI_AP || mode == WIFI_AP_STA) && WiFi.softAPIP() != IPAddress(0, 0, 0, 0);
    if (WiFi.status() != WL_CONNECTED && !apReady) return;
    if (!running) {
      server.begin();
      running = true;
      rt->webRunning = true;
      IPAddress ip = apReady ? WiFi.softAPIP() : WiFi.localIP();
      String ipstr = ip.toString();
      strncpy(rt->webIp, ipstr.c_str(), sizeof(rt->webIp) - 1);
      rt->webIp[sizeof(rt->webIp) - 1] = 0;
      strncpy(rt->webStatus, "RUNNING", sizeof(rt->webStatus) - 1);
      rt->webStatus[sizeof(rt->webStatus) - 1] = 0;
      Serial.printf("[WEB] http://%s/ started (all commands enabled)\n", rt->webIp);
    }
    server.handleClient();
  }

  void print() const {
    Serial.printf("[WEB] enabled=%d running=%d ip=%s port=%u requests=%lu ticks=%lu gap=%lu/%lums task=%d loops=%lu stack=%lu queue=%u/%u accepted=%lu rejected=%lu last=%s status=%s\n",
      rt && rt->webEnabled ? 1 : 0, running ? 1 : 0, rt ? rt->webIp : "", rt ? rt->webPort : 0,
      (unsigned long)requests, (unsigned long)(rt ? rt->webTicks : 0),
      (unsigned long)(rt ? rt->webTickGapLastMs : 0), (unsigned long)(rt ? rt->webTickGapMaxMs : 0),
      rt && rt->webTaskRunning ? 1 : 0, (unsigned long)(rt ? rt->webTaskLoops : 0),
      (unsigned long)(rt ? rt->webTaskStackHighWater : 0),
      (unsigned)commandCount, (unsigned)COMMAND_QUEUE_SIZE,
      (unsigned long)commandAccepted, (unsigned long)commandRejected, lastExecuted,
      rt ? rt->webStatus : "NA");
  }
};
