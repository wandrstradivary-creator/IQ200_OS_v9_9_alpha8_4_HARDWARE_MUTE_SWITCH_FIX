#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "../drivers/Display.h"
#include "../drivers/Encoder.h"
#include "../services/SystemService.h"
#include "../services/WifiService.h"
#include "../services/StorageService.h"
#include "../services/AudioEngine.h"
#include "../services/AppManager.h"
#include "WindowManager.h"
#include "ThemeManager.h"
#include "NotificationManager.h"
#include "Renderer.h"
#include "WidgetEngine.h"
#include "ListView.h"
#include "StatusBar.h"
#include "Popup.h"
#include "../services/RuntimeState.h"
#include "../services/ArtworkCache.h"
#include "../services/MediaEngine.h"
#include "Font5x7.h"
#include "PlayerLayout.h"
#include "iq200_pins.h"

enum Screen {
  SCR_HOME,
  SCR_STATUS,
  SCR_DISPLAY,
  SCR_PSRAM,
  SCR_ENCODERS,
  SCR_AUDIO,
  SCR_WIFI,
  SCR_SD,
  SCR_SETTINGS,
  SCR_PLAYER,
  SCR_FILES,
  SCR_LIBRARY,
  SCR_ALBUMS,
  SCR_FAVORITES
};

class UI {
  IQ200Display& tft;
  LGFX_Sprite fb;

  SystemService& sys;
  WifiService& wifi;
  StorageService& storage;
  AudioEngine& audio;
  RuntimeState& rt;
  AppManager& apps;
  WindowManager windows;
  ThemeManager themes;
  NotificationManager notifications;
  RendererStats renderer;
  WidgetEngine widgets;
  ListView listView;
  StatusBar statusBar;
  Popup popup;

  Encoder nav;
  Encoder volEnc;

  Screen screen = SCR_HOME;
  int sel = 0;
  int vol = 42;
  bool fbReady = false;
  String msg = "Готово";

  // v8.2.5 Partial Renderer cache. Full screens are still rendered into PSRAM FB,
  // but high-frequency player widgets are repainted directly on the ILI9488.
  int lastPlayerPercent = -1;
  int lastPlayerProgressFill = -1;
  int lastPlayerProgressTextPercent = -1;
  int lastPlayerVol = -1;
  uint8_t lastPlayerVuL = 255;
  uint8_t lastPlayerVuR = 255;
  uint8_t lastPlayerPeakL = 255;
  uint8_t lastPlayerPeakR = 255;
  uint8_t playerPeakL = 0;
  uint8_t playerPeakR = 0;
  uint32_t playerPeakHoldUntilL = 0;
  uint32_t playerPeakHoldUntilR = 0;
  uint32_t playerPeakLastMs = 0;
  bool lastPlayerPlaying = false;
  uint32_t lastPlayerBytes = 0;
  uint32_t lastPlayerUnderruns = 0;
  uint8_t lastPlayerState = 255;
  char lastPlayerTitle[128] = "";
  int16_t playerTitleScrollX = 0;
  int16_t playerTitleTextW = 0;
  uint32_t playerTitleScrollMs = 0;
  uint32_t playerTitlePauseUntil = 0;
  bool playerTitleScrollReturning = false;
  static constexpr uint8_t PLAYER_TITLE_SCALE = 3;
  static constexpr uint16_t PLAYER_TITLE_STEP_MS = 250;
  static constexpr uint16_t PLAYER_TITLE_PAUSE_MS = 2000;
  static constexpr uint8_t PLAYER_TITLE_STEP_PX = 4;
  uint32_t lastArtworkGeneration = 0xffffffffUL;
  uint32_t failedArtworkGeneration = 0xffffffffUL;
  uint32_t lastArtworkRetryMs = 0;
  uint32_t lastPartialMs = 0;
  uint32_t lastVuMs = 0;
  uint32_t lastProgressMs = 0;
  uint32_t lastHealthMs = 0;

  // v9.2-alpha12: 20 FPS compact VU + Peak Hold with delta rendering.
  static constexpr uint16_t UI_TARGET_FPS = 20;
  static constexpr uint32_t PLAYER_FRAME_MS = 10;       // 100 Hz lightweight scheduler ceiling
  static constexpr uint32_t PLAYER_PROGRESS_MS = 1000;  // 1 FPS
  static constexpr uint32_t PLAYER_HEALTH_MS = 60000;   // disabled during playback
  static constexpr uint8_t PLAYER_VU_DELTA = 2;         // suppress one-step LCD noise
  static constexpr uint8_t PLAYER_VU_SEGMENTS_DEFAULT = 12;
  static constexpr uint8_t PLAYER_VU_SEGMENTS_MIN = 4;
  static constexpr uint8_t PLAYER_VU_SEGMENTS_MAX = 24;
  enum VuStyle : uint8_t { VU_RECT = 0, VU_DOT = 1, VU_THIN = 2, VU_LINE = 3 };
  uint8_t playerVuSegments = PLAYER_VU_SEGMENTS_DEFAULT;
  VuStyle playerVuStyle = VU_RECT;
  uint8_t playerVuFps = 20;
  bool playerVuPeakEnabled = true;
  uint16_t playerVuPeakHoldMs = 500;
  uint8_t playerVuDecay = 3; // 1..10
  PlayerLayoutMode playerLayoutMode = PLAYER_LAYOUT_ARTWORK;

  const char* playerLayoutName() const {
    switch (playerLayoutMode) {
      case PLAYER_LAYOUT_DUAL_VU: return "dual";
      case PLAYER_LAYOUT_ARTWORK: return "artwork";
      case PLAYER_LAYOUT_CLASSIC: return "classic";
      default: return "modern";
    }
  }

  void loadPlayerLayout() {
    // v9.8-alpha16: audio-safe marquee; Artwork Focus remains canonical.
    // Ignore older stored layouts so every device boots into the exact 480x320 geometry.
    playerLayoutMode = PLAYER_LAYOUT_ARTWORK;
  }

  void savePlayerLayout() {
    Preferences p;
    if (!p.begin("iq200_player", false)) return;
    p.putUChar("layout", (uint8_t)playerLayoutMode);
    p.end();
  }

  const char* vuStyleName() const {
    switch (playerVuStyle) {
      case VU_DOT: return "dot";
      case VU_THIN: return "thin";
      case VU_LINE: return "line";
      default: return "rect";
    }
  }

  uint32_t playerVuIntervalMs() const {
    return 1000UL / constrain((int)playerVuFps, 10, 30);
  }

  void loadVuSettings() {
    Preferences p;
    if (!p.begin("iq200_vu", true)) return;
    playerVuSegments = constrain((int)p.getUChar("segments", PLAYER_VU_SEGMENTS_DEFAULT),
                                 (int)PLAYER_VU_SEGMENTS_MIN, (int)PLAYER_VU_SEGMENTS_MAX);
    playerVuStyle = (VuStyle)constrain((int)p.getUChar("style", VU_RECT), 0, 3);
    playerVuFps = constrain((int)p.getUChar("fps", 20), 10, 30);
    playerVuPeakEnabled = p.getBool("peak", true);
    playerVuPeakHoldMs = constrain((int)p.getUShort("hold", 500), 50, 1500);
    playerVuDecay = constrain((int)p.getUChar("decay", 3), 1, 10);
    p.end();
  }

  void saveVuSettings() {
    Preferences p;
    if (!p.begin("iq200_vu", false)) return;
    p.putUChar("segments", playerVuSegments);
    p.putUChar("style", (uint8_t)playerVuStyle);
    p.putUChar("fps", playerVuFps);
    p.putBool("peak", playerVuPeakEnabled);
    p.putUShort("hold", playerVuPeakHoldMs);
    p.putUChar("decay", playerVuDecay);
    p.end();
  }

  void printVuStatus() const {
    Serial.printf("[VU] style=%s segments=%u fps=%u peak=%s hold=%ums decay=%u\n",
                  vuStyleName(), playerVuSegments, playerVuFps,
                  playerVuPeakEnabled ? "on" : "off",
                  playerVuPeakHoldMs, playerVuDecay);
  }

  struct UIProfileCounter {
    uint32_t draws = 0;
    uint64_t totalUs = 0;
    uint32_t maxUs = 0;
    void add(uint32_t us) { draws++; totalUs += us; if (us > maxUs) maxUs = us; }
    void clear() { draws = 0; totalUs = 0; maxUs = 0; }
  };

  UIProfileCounter profTitle, profState, profVolume, profVu, profProgress, profArtwork, profFull;

  template <typename F>
  void profileDraw(UIProfileCounter& counter, F fn) {
    const uint32_t startUs = micros();
    fn();
    counter.add((uint32_t)(micros() - startUs));
  }

  uint16_t fps = 0;
  uint16_t frames = 0;
  uint32_t lastFpsMs = 0;
  uint32_t lastUiFrameTotal = 0;

  static constexpr uint8_t HOME_ITEM_COUNT = 5;
  const char* homeItems[HOME_ITEM_COUNT] = {
    "PLAYER", "WEB RADIO", "RADIO", "BLUETOOTH", "SETTINGS"
  };

public:
  UI(IQ200Display& d, SystemService& s, WifiService& w, StorageService& st, AudioEngine& a, RuntimeState& r, AppManager& appMgr)
    : tft(d), fb(&d), sys(s), wifi(w), storage(st), audio(a), rt(r), apps(appMgr),
      nav(IQ200_ENC_NAV_CLK, IQ200_ENC_NAV_DT, IQ200_ENC_NAV_SW),
      volEnc(IQ200_ENC_VOL_CLK, IQ200_ENC_VOL_DT, IQ200_ENC_VOL_SW) {}

  void printProfiler(Stream& out = Serial) const {
    auto row = [&](const char* name, const UIProfileCounter& p) {
      const uint32_t avg = p.draws ? (uint32_t)(p.totalUs / p.draws) : 0;
      out.printf("%-10s draws=%lu avg=%luus max=%luus\n", name, (unsigned long)p.draws, (unsigned long)avg, (unsigned long)p.maxUs);
    };
    out.println("========== UI PROFILER ==========");
    out.printf("screen=%u fps=%u full=%lu partial=%lu dirty=%lu\n", (unsigned)screen, (unsigned)fps, (unsigned long)renderer.fullFrames, (unsigned long)renderer.partialFrames, (unsigned long)renderer.dirtyFrames);
    row("full", profFull); row("title", profTitle); row("state", profState); row("volume", profVolume);
    row("vu", profVu); row("progress", profProgress); row("artwork", profArtwork);
    out.println("=================================");
  }

  void resetProfiler() {
    profFull.clear(); profTitle.clear(); profState.clear(); profVolume.clear();
    profVu.clear(); profProgress.clear(); profArtwork.clear();
    Serial.println("[UI] profiler reset");
  }

  void begin() {
    nav.begin();
    volEnc.begin();
    themes.begin();
    Serial.printf("[THEME] loaded=%s\n", themes.name());
    loadVuSettings();
    loadPlayerLayout();
    printVuStatus();
    Serial.printf("[PLAYER-UI] layout=%s saved=1\n", playerLayoutName());
    vol = rt.volumePercent;
    if (vol < 0) vol = 0;
    if (vol > 100) vol = 100;

    fb.setPsram(true);
    fb.setColorDepth(16);
    fbReady = fb.createSprite(480, 320) != nullptr;

    Serial.printf("[UI] framebuffer: %s\n", fbReady ? "PSRAM OK" : "FAIL");

    splash();
    home();
  }

  void present() {
    const uint32_t profileStartUs = micros();
    renderer.beginFrame();
    if (!fbReady) return;
    tft.startWrite();
    fb.pushSprite(0, 0);
    tft.endWrite();

    frames++;
    renderer.endFrame(true);
    rt.rendererFrames = renderer.frameCount;
    rt.dirtyFrames = renderer.dirtyFrames;
    rt.partialFrames = renderer.partialFrames;
    rt.fullFrames = renderer.fullFrames;
    profFull.add((uint32_t)(micros() - profileStartUs));
  }

  void text(int x, int y, const char* s, uint16_t fg, uint16_t bg, int scale) {
    iqDrawText(fb, x, y, s, fg, bg, scale);
  }

  const char* playerTitle() const {
    return rt.mediaTitle[0] ? rt.mediaTitle : (rt.playlistCurrent[0] ? rt.playlistCurrent : "No track");
  }

  void playerPathParts(String& artist, String& album) const {
    String path = rt.mediaPath[0] ? String(rt.mediaPath) : String(rt.playlistCurrent);
    path.replace("\\", "/");
    int fileSlash = path.lastIndexOf('/');
    if (fileSlash < 0) { artist = "Unknown artist"; album = "Root"; return; }
    String dir = path.substring(0, fileSlash);
    int albumSlash = dir.lastIndexOf('/');
    album = albumSlash >= 0 ? dir.substring(albumSlash + 1) : dir;
    String parent = albumSlash > 0 ? dir.substring(0, albumSlash) : String();
    int artistSlash = parent.lastIndexOf('/');
    artist = artistSlash >= 0 ? parent.substring(artistSlash + 1) : parent;
    if (!artist.length() || artist.equalsIgnoreCase("Music")) artist = album;
    if (!album.length()) album = "Unknown album";
    if (!artist.length()) artist = "Unknown artist";
  }

  uint32_t playerElapsedSeconds() const {
    const uint32_t rate = rt.mediaSampleRate;
    const uint32_t bytesPerSample = rt.mediaBits >= 8 ? (uint32_t)rt.mediaBits / 8U : 1U;
    const uint32_t frameBytes = (uint32_t)rt.mediaChannels * bytesPerSample;
    if (!rate || !frameBytes) return 0;
    return rt.mediaPlayedBytes / (rate * frameBytes);
  }

  uint32_t playerDurationSeconds() const {
    const uint32_t rate = rt.mediaSampleRate;
    const uint32_t bytesPerSample = rt.mediaBits >= 8 ? (uint32_t)rt.mediaBits / 8U : 1U;
    const uint32_t frameBytes = (uint32_t)rt.mediaChannels * bytesPerSample;
    if (!rate || !frameBytes) return 0;
    return rt.mediaDataSize / (rate * frameBytes);
  }

  void formatPlayerTime(uint32_t sec, char* out, size_t outLen) const {
    snprintf(out, outLen, "%02lu:%02lu", (unsigned long)(sec / 60UL), (unsigned long)(sec % 60UL));
  }

  const char* playerCodecName() const {
    switch (rt.mediaCodec) {
      case MEDIA_CODEC_WAV: return "WAV";
      case MEDIA_CODEC_MP3: return "MP3";
      case MEDIA_CODEC_FLAC: return "FLAC";
      default: return "---";
    }
  }

  uint8_t playerProgress() const {
    int p = rt.mediaProgress;
    if (rt.mediaCodec == MEDIA_CODEC_WAV && rt.wavProgress > p) p = rt.wavProgress;
    if (p < 0) p = 0;
    if (p > 100) p = 100;
    return (uint8_t)p;
  }

  uint8_t playerVuL() const { return rt.mediaVuLeft ? rt.mediaVuLeft : rt.wavVuLeft; }
  uint8_t playerVuR() const { return rt.mediaVuRight ? rt.mediaVuRight : rt.wavVuRight; }

  const char* playerStateName() const {
    if (rt.audioPlaying || rt.rtAudioTaskRunning) return "PLAYING";
    return MediaEngine::stateName(rt.mediaState);
  }

  void splash() {
    fb.fillScreen(themes.get().bg);
    fb.drawRoundRect(70, 75, 340, 150, 16, themes.get().accent);
    fb.fillRoundRect(75, 80, 330, 140, 14, themes.get().panel);
    fb.setTextSize(3);
    fb.setTextColor(themes.get().text, themes.get().panel);
    fb.setCursor(145, 105);
    fb.print("IQ200 OS");
    text(130, 155, "версія 9.1.3", themes.get().accent, themes.get().panel, 3);
    fb.fillRect(110, 205, 260, 12, themes.get().border);
    fb.fillRect(110, 205, 260, 12, themes.get().ok);
    present();
    delay(700);
  }

  void header(const char* title) {
    fb.fillScreen(themes.get().bg);
    fb.fillRect(0, 0, 480, 42, themes.get().panel);
    fb.setTextSize(2);
    fb.setTextColor(themes.get().text, themes.get().panel);
    fb.setCursor(10, 13);
    fb.print("IQ200 OS v9.8-alpha16");
    text(260, 12, title, themes.get().accent, themes.get().panel, 2);
  }

  void footer(const char* s) {
    fb.fillRect(0, 282, 480, 38, themes.get().panel);
    text(10, 292, s, themes.get().text, themes.get().panel, 2);
    fb.setTextSize(1);
    fb.setTextColor(themes.get().dim, themes.get().panel);
    fb.setCursor(330, 306);
    fb.printf("FPS:%u/%u W:%d", fps, UI_TARGET_FPS, windows.depth());
  }

  void icon(int x, int y, uint16_t c, int type) {
    fb.fillRoundRect(x, y, 22, 22, 5, c);
    fb.drawRoundRect(x, y, 22, 22, 5, themes.get().text);
    fb.setTextSize(1);
    fb.setTextColor(themes.get().bg, c);
    fb.setCursor(x + 5, y + 7);
    const char* sym[] = {"S","D","P","E","A","W","SD","F","M","N"};
    fb.print(sym[type]);
  }

  void volumeBox() {
    fb.drawRoundRect(342, 82, 110, 140, 8, themes.get().peak);
    fb.setTextSize(2);
    fb.setTextColor(themes.get().warn, themes.get().bg);
    fb.setCursor(374, 102); fb.print("VOL");
    fb.setCursor(386, 136); fb.printf("%d", vol);
    fb.fillRect(365, 180, 65, 18, themes.get().border);
    fb.fillRect(365, 180, map(vol, 0, 100, 0, 65), 18, themes.get().ok);
  }

  enum HomeIcon : uint8_t { HOME_PLAYER = 0, HOME_WEB_RADIO, HOME_RADIO, HOME_BLUETOOTH, HOME_SETTINGS };

  uint16_t homeIconColor(uint8_t index) const {
    switch (index) {
      case HOME_WEB_RADIO: return 0x981F; // violet
      case HOME_RADIO: return 0xFD20;     // orange
      case HOME_BLUETOOTH: return 0x04FF; // cyan-blue
      case HOME_SETTINGS: return 0x07E0;  // green
      default: return 0x057F;             // blue
    }
  }

  void drawHomeIcon(int x, int y, uint8_t index, uint16_t color) {
    const int cx = x + 38;
    const int cy = y + 36;
    fb.fillRoundRect(x, y, 76, 76, 14, themes.get().panel);
    fb.drawRoundRect(x, y, 76, 76, 14, color);

    switch (index) {
      case HOME_PLAYER: {
        fb.drawLine(cx + 8, cy - 24, cx + 8, cy + 13, color);
        fb.drawLine(cx + 8, cy - 24, cx + 25, cy - 29, color);
        fb.drawLine(cx + 25, cy - 29, cx + 25, cy + 6, color);
        fb.fillCircle(cx + 1, cy + 18, 9, color);
        fb.fillCircle(cx + 18, cy + 11, 9, color);
        break;
      }
      case HOME_WEB_RADIO: {
        fb.drawCircle(cx - 4, cy + 2, 20, color);
        fb.drawLine(cx - 24, cy + 2, cx + 16, cy + 2, color);
        fb.drawLine(cx - 4, cy - 18, cx - 4, cy + 22, color);
        fb.drawEllipse(cx - 4, cy + 2, 9, 20, color);
        fb.drawArc(cx + 18, cy - 8, 16, 16, 295, 65, color);
        fb.drawArc(cx + 18, cy - 8, 23, 23, 300, 60, color);
        break;
      }
      case HOME_RADIO: {
        fb.fillRoundRect(cx - 25, cy - 10, 50, 34, 6, color);
        fb.drawLine(cx - 19, cy - 10, cx + 20, cy - 30, themes.get().text);
        fb.drawLine(cx - 17, cy, cx + 2, cy, themes.get().bg);
        fb.drawLine(cx - 17, cy + 7, cx + 2, cy + 7, themes.get().bg);
        fb.drawLine(cx - 17, cy + 14, cx + 2, cy + 14, themes.get().bg);
        fb.fillCircle(cx + 15, cy + 7, 8, themes.get().bg);
        fb.drawCircle(cx + 15, cy + 7, 8, themes.get().text);
        break;
      }
      case HOME_BLUETOOTH: {
        fb.drawLine(cx, cy - 28, cx, cy + 28, color);
        fb.drawLine(cx, cy - 28, cx + 19, cy - 10, color);
        fb.drawLine(cx + 19, cy - 10, cx - 13, cy + 17, color);
        fb.drawLine(cx - 13, cy - 17, cx + 19, cy + 10, color);
        fb.drawLine(cx + 19, cy + 10, cx, cy + 28, color);
        break;
      }
      case HOME_SETTINGS: {
        fb.drawCircle(cx, cy, 23, color);
        fb.drawCircle(cx, cy, 9, color);
        for (int i = 0; i < 8; ++i) {
          const float a = i * 0.785398f;
          const int x1 = cx + (int)(23 * cosf(a));
          const int y1 = cy + (int)(23 * sinf(a));
          const int x2 = cx + (int)(30 * cosf(a));
          const int y2 = cy + (int)(30 * sinf(a));
          fb.drawLine(x1, y1, x2, y2, color);
          fb.fillCircle(x2, y2, 3, color);
        }
        break;
      }
    }
  }

  void drawHomeTile(uint8_t index, int x) {
    const bool active = index == sel;
    const uint16_t color = homeIconColor(index);
    if (active) {
      fb.fillRoundRect(x - 3, 70, 82, 116, 16, themes.get().buttonActive);
      fb.drawRoundRect(x - 3, 70, 82, 116, 16, color);
    }
    drawHomeIcon(x, 76, index, color);

    fb.setTextSize(1);
    fb.setTextColor(active ? themes.get().text : themes.get().dim,
                    active ? themes.get().buttonActive : themes.get().bg);
    int labelX = x + 38 - (int)strlen(homeItems[index]) * 3;
    if (labelX < x - 2) labelX = x - 2;
    fb.setCursor(labelX, 160);
    fb.print(homeItems[index]);

    if (active) fb.fillRoundRect(x + 18, 179, 40, 4, 2, color);
  }

  void drawHomeWifi() {
    const uint16_t c = rt.wifiConnected ? themes.get().ok : themes.get().dim;
    fb.drawArc(24, 20, 18, 18, 220, 320, c);
    fb.drawArc(24, 20, 12, 12, 220, 320, c);
    fb.fillCircle(24, 25, 2, c);
  }

  void drawHomeVolume() {
    const int x = 423;
    const int y = 14;
    fb.fillTriangle(x, y + 6, x + 8, y + 6, x + 15, y, themes.get().text);
    fb.fillTriangle(x, y + 6, x + 8, y + 6, x + 15, y + 12, themes.get().text);
    if (vol > 0) fb.drawArc(x + 13, y + 6, 10, 10, 300, 60, themes.get().text);
    fb.setTextSize(2);
    fb.setTextColor(themes.get().text, themes.get().panel);
    fb.setCursor(454, 12);
    fb.printf("%d", vol);
  }

  void home() {
    screen = SCR_HOME;
    fb.fillScreen(themes.get().bg);
    fb.fillRect(0, 0, 480, 42, themes.get().panel);
    fb.drawFastHLine(0, 41, 480, themes.get().border);

    drawHomeWifi();
    fb.setTextSize(2);
    fb.setTextColor(themes.get().text, themes.get().panel);
    fb.setCursor(211, 12);
    fb.print("HOME");
    drawHomeVolume();

    const int tileX[HOME_ITEM_COUNT] = {7, 101, 195, 289, 383};
    for (uint8_t i = 0; i < HOME_ITEM_COUNT; ++i) drawHomeTile(i, tileX[i]);

    fb.setTextSize(1);
    fb.setTextColor(themes.get().dim, themes.get().bg);
    fb.setCursor(18, 236);
    fb.print("ROTATE ENCODER TO SELECT");
    fb.setCursor(315, 236);
    fb.print("PRESS TO OPEN");

    fb.drawFastHLine(0, 270, 480, themes.get().border);
    fb.setCursor(16, 286);
    fb.printf("%s", msg.c_str());
    present();
  }

  void status() {
    appOpened(APP_STATUS);
    screen = SCR_STATUS;
    header("СТАТУС");
    text(25, 62, "Система працює", themes.get().ok, themes.get().bg, 3);
    fb.setTextSize(2);
    fb.setTextColor(themes.get().text, themes.get().bg);
    fb.setCursor(30, 120); fb.printf("Flash: %lu MB", sys.flashMB());
    fb.setCursor(30, 150); fb.printf("Heap : %lu", sys.heapFree());
    fb.setCursor(30, 180); fb.printf("PSRAM: %lu KB", sys.psramKB());
    fb.setCursor(30, 210); fb.printf("Uptime: %lu sec", sys.uptimeSec());
    fb.setCursor(30, 240); fb.printf("C0:%lu C1:%lu", (unsigned long)rt.core0Loops, (unsigned long)rt.core1Loops);
    fb.setCursor(260, 240); fb.printf("APP:%lu", (unsigned long)rt.appSwitches);
    footer("OK - назад");
    present();
  }

  void taskMonitor() {
    appOpened(APP_TASKS);
    screen = SCR_STATUS;
    header("TASKS");
    text(25, 58, "Core monitor", themes.get().accent, themes.get().bg, 3);
    fb.setTextSize(2);
    fb.setTextColor(themes.get().text, themes.get().bg);
    fb.setCursor(25, 115); fb.printf("C0 stack: %lu", (unsigned long)rt.core0StackHighWater);
    fb.setCursor(25, 145); fb.printf("C1 stack: %lu", (unsigned long)rt.core1StackHighWater);
    fb.setCursor(25, 175); fb.printf("Heap: %lu", (unsigned long)ESP.getFreeHeap());
    fb.setCursor(25, 205); fb.printf("Drops: %lu", (unsigned long)rt.eventQueueDrops);
    fb.setCursor(25, 220); fb.printf("Age C0:%lu C1:%lu", (unsigned long)rt.core0AgeMs, (unsigned long)rt.core1AgeMs);
    fb.setCursor(25, 235); fb.printf("MSG: %s", rt.lastMessage);
    footer("OK - назад");
    present();
  }

  void displayTest() {
    appOpened(APP_DISPLAY);
    screen = SCR_DISPLAY;
    tft.fillScreen(themes.get().error); delay(120);
    tft.fillScreen(themes.get().ok); delay(120);
    tft.fillScreen(themes.get().progress); delay(120);
    tft.fillScreen(themes.get().text); delay(120);

    header("ДИСПЛЕЙ");
    fb.drawRect(0, 42, 479, 277, themes.get().warn);
    fb.fillRect(30, 80, 80, 60, themes.get().error);
    fb.fillRect(130, 80, 80, 60, themes.get().ok);
    fb.fillRect(230, 80, 80, 60, themes.get().progress);
    text(30, 170, "Екран працює", themes.get().accent, themes.get().bg, 3);
    fb.setTextSize(2);
    fb.setTextColor(themes.get().text, themes.get().bg);
    fb.setCursor(30, 225); fb.print("ILI9488 SPI 40 MHz");
    footer("OK - назад");
    present();
  }

  void psram() {
    appOpened(APP_PSRAM);
    screen = SCR_PSRAM;
    header("PSRAM");
    text(25, 62, "Память PSRAM", themes.get().accent, themes.get().bg, 3);
    uint32_t total = sys.psramKB();
    uint32_t freep = sys.psramFreeKB();
    fb.setTextSize(2);
    fb.setTextColor(themes.get().text, themes.get().bg);
    fb.setCursor(30, 120); fb.printf("Total: %lu KB", total);
    fb.setCursor(30, 150); fb.printf("Free : %lu KB", freep);
    fb.setCursor(30, 180); fb.print("Framebuffer: 300 KB");
    fb.drawRect(30, 220, 360, 24, themes.get().text);
    if (total) fb.fillRect(32, 222, map(freep, 0, total, 0, 356), 20, themes.get().ok);
    footer("OK - назад");
    present();
  }

  void encoders() {
    appOpened(APP_ENCODERS);
    screen = SCR_ENCODERS;
    header("ЕНКОДЕРИ");
    text(25, 62, "Крути енкодери", themes.get().accent, themes.get().bg, 3);
    fb.setTextSize(2);
    fb.setTextColor(themes.get().text, themes.get().bg);
    fb.setCursor(30, 130); fb.printf("NAV: CLK=%d DT=%d SW=%d", digitalRead(IQ200_ENC_NAV_CLK), digitalRead(IQ200_ENC_NAV_DT), digitalRead(IQ200_ENC_NAV_SW));
    fb.setCursor(30, 165); fb.printf("VOL: CLK=%d DT=%d SW=%d", digitalRead(IQ200_ENC_VOL_CLK), digitalRead(IQ200_ENC_VOL_DT), digitalRead(IQ200_ENC_VOL_SW));
    fb.setCursor(30, 200); fb.printf("Volume: %d", vol);
    footer("OK - назад");
    present();
  }

  void audioScreen() {
    appOpened(APP_AUDIO);
    screen = SCR_AUDIO;
    header("АУДІО");
    text(30, 65, "PCM5102 / I2S", themes.get().accent, themes.get().bg, 3);
    bool ok = audio.begin();
    audio.setVolume(vol);
    fb.setTextSize(2);
    fb.setTextColor(ok ? themes.get().ok : themes.get().error, themes.get().bg);
    fb.setCursor(30, 135); fb.printf("I2S: %s", ok ? "OK" : "FAIL");
    fb.setTextColor(themes.get().text, themes.get().bg);
    fb.setCursor(30, 170); fb.print("BCK16 LRCK18 DOUT17");
    fb.setCursor(30, 205); fb.printf("Volume: %d", vol);
    footer("OK/back Home  stop=STOP");
    present();
  }

  void wifiScreen() {
    appOpened(APP_WIFI);
    screen = SCR_WIFI;
    header("WIFI");

    if (!rt.wifiScanBusy && rt.wifiNetworks < 0) {
      rt.wifiScanRequest = true;
    }

    if (rt.wifiScanBusy || rt.wifiScanRequest) {
      text(30, 62, "Сканування WiFi", themes.get().accent, themes.get().bg, 3);
      fb.setTextSize(2);
      fb.setTextColor(themes.get().warn, themes.get().bg);
      fb.setCursor(30, 120); fb.print("Core0 scanning...");
      footer("OK - назад");
      present();
      return;
    }

    fb.setTextSize(2);
    fb.setTextColor(themes.get().text, themes.get().bg);
    fb.setCursor(25, 62);
    fb.printf("Networks: %d", rt.wifiNetworks);

    int n = rt.wifiNetworks;
    for (int i = 0; i < n && i < 5; i++) {
      fb.setCursor(25, 95 + i * 30);
      String ssid = WiFi.SSID(i);
      if (ssid.length() > 18) ssid = ssid.substring(0, 18);
      fb.printf("%d %s %d", i + 1, ssid.c_str(), WiFi.RSSI(i));
    }

    footer("OK - назад");
    present();
  }

  void sdScreen() {
    appOpened(APP_SD);
    screen = SCR_SD;
    header("SD");
    text(30, 62, "SD карта GPIO38", themes.get().accent, themes.get().bg, 3);

    if (!rt.sdBusy && !rt.sdOk) {
      rt.sdMountRequest = true;
    }

    fb.setTextSize(2);
    if (rt.sdBusy || rt.sdMountRequest) {
      fb.setTextColor(themes.get().warn, themes.get().bg);
      fb.setCursor(30, 125); fb.print("Core0 mounting...");
    } else if (rt.sdOk) {
      fb.setTextColor(themes.get().ok, themes.get().bg);
      fb.setCursor(30, 125); fb.print("SD: OK");
      fb.setTextColor(themes.get().text, themes.get().bg);
      fb.setCursor(30, 160); fb.printf("Size: %llu MB", rt.sdMB);
    } else {
      fb.setTextColor(themes.get().error, themes.get().bg);
      fb.setCursor(30, 125); fb.print("SD: NOT FOUND");
    }

    footer("OK - назад");
    present();
  }

  void filesScreen() {
    screen = SCR_FILES;
    appOpened(APP_FILES);
    header("ФАЙЛИ");

    if (!rt.indexBusy && rt.fileIndexCount == 0) {
      rt.indexRequest = true;
    }

    text(25, 58, "SD File Index", themes.get().accent, themes.get().bg, 3);

    fb.setTextSize(2);
    fb.setTextColor(themes.get().text, themes.get().bg);

    if (rt.indexBusy || rt.indexRequest) {
      fb.setCursor(25, 125);
      fb.print("Core0 indexing...");
    } else {
      fb.setCursor(25, 100);
      fb.printf("Files: %d", rt.fileIndexCount);

      listView.row(fb, 25, 135, 360, "Root index ready", true);
      listView.row(fb, 25, 165, 360, "Use serial: index", false);
      listView.row(fb, 25, 195, 360, "WAV player next", false);
    }

    footer("OK - назад");
    present();
  }


  void drawVUMeter(int x, int y, int progress) {
    // v7.1 RT Audio: VU is computed in the audio task and UI draws only two small bars.
    uint8_t l = playerVuL();
    uint8_t r = playerVuR();
    if (!playerVuPeakEnabled) {
      playerPeakL = playerPeakR = 0;
      playerPeakHoldUntilL = playerPeakHoldUntilR = 0;
      return;
    }

    if (!rt.audioPlaying && !rt.rtAudioTaskRunning) {
      l = 0;
      r = 0;
    }

    fb.setTextSize(1);
    fb.setTextColor(themes.get().border, themes.get().bg);
    fb.setCursor(x, y - 2); fb.print("L");
    fb.setCursor(x, y + 22); fb.print("R");

    int w = 88;
    int lh = map(l, 0, 100, 0, w);
    int rh = map(r, 0, 100, 0, w);

    fb.drawRect(x + 14, y, w, 10, themes.get().border);
    fb.drawRect(x + 14, y + 24, w, 10, themes.get().border);
    fb.fillRect(x + 16, y + 2, lh > w - 4 ? w - 4 : lh, 6, themes.get().ok);
    fb.fillRect(x + 16, y + 26, rh > w - 4 ? w - 4 : rh, 6, themes.get().ok);
  }

  void drawPlayerButton(int x, int y, int w, int h, const char* label, uint16_t border, uint16_t txt) {
    fb.fillRoundRect(x, y, w, h, 8, themes.get().bg);
    fb.drawRoundRect(x, y, w, h, 8, border);
    fb.setTextSize(2);
    fb.setTextColor(txt, themes.get().bg);
    fb.setCursor(x + 18, y + 16);
    fb.print(label);
  }

  void drawProgressBar(int x, int y, int w, int h, int percent) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    fb.drawRoundRect(x, y, w, h, 8, themes.get().text);
    fb.fillRoundRect(x + 3, y + 3, w - 6, h - 6, 6, themes.get().bg);
    int fillw = (w - 8) * percent / 100;
    if (fillw > 0) fb.fillRoundRect(x + 4, y + 4, fillw, h - 8, 5, themes.get().progress);
  }

  void resetPlayerPartialCache() {
    lastPlayerPercent = -1;
    lastPlayerProgressFill = -1;
    lastPlayerProgressTextPercent = -1;
    lastPlayerVol = -1;
    lastPlayerVuL = 255;
    lastPlayerVuR = 255;
    lastPlayerPeakL = 255;
    lastPlayerPeakR = 255;
    playerPeakL = 0;
    playerPeakR = 0;
    playerPeakHoldUntilL = 0;
    playerPeakHoldUntilR = 0;
    playerPeakLastMs = millis();
    lastPlayerPlaying = !rt.audioPlaying;
    lastPlayerBytes = 0xffffffff;
    lastPlayerUnderruns = 0xffffffff;
    lastPlayerState = 255;
    lastPlayerTitle[0] = 0;
    playerTitleScrollX = 0;
    playerTitleTextW = 0;
    playerTitleScrollMs = 0;
    playerTitlePauseUntil = 0;
    playerTitleScrollReturning = false;
    lastArtworkGeneration = 0xffffffffUL;
  }

  // v9.5-alpha2: delta-only progress renderer. The static frame is kept on TFT;
  // only the newly filled/cleared strip and percentage text are updated.
  void drawDirectProgress(int percent) {
    percent = constrain(percent, 0, 100);
    const UIRect& r = iq200PlayerLayout(playerLayoutMode).progress;
    const int innerX = r.x + 2;
    const int innerY = r.y + 2;
    const int innerW = max(0, (int)r.w - 4);
    const int innerH = max(0, (int)r.h - 4);
    const int fillw = (innerW * percent) / 100;

    tft.startWrite();

    if (lastPlayerProgressFill < 0) {
      // First draw after entering Player/theme refresh: initialize once.
      tft.fillRect(r.x, r.y, r.w, r.h, themes.get().bg);
      tft.drawRect(r.x, r.y, r.w, r.h, themes.get().border);
      if (fillw > 0 && innerH > 0) {
        tft.fillRect(innerX, innerY, fillw, innerH, themes.get().progress);
      }
    } else if (fillw > lastPlayerProgressFill) {
      // Forward playback: draw only the newly completed pixels.
      tft.fillRect(innerX + lastPlayerProgressFill, innerY,
                   fillw - lastPlayerProgressFill, innerH,
                   themes.get().progress);
    } else if (fillw < lastPlayerProgressFill) {
      // Seek/restart: erase only the pixels no longer completed.
      tft.fillRect(innerX + fillw, innerY,
                   lastPlayerProgressFill - fillw, innerH,
                   themes.get().bg);
    }

    // Replace percentage with elapsed and total time around the progress bar.
    if (lastPlayerProgressTextPercent != percent) {
      char elapsedText[12], totalText[12];
      formatPlayerTime(playerElapsedSeconds(), elapsedText, sizeof(elapsedText));
      formatPlayerTime(playerDurationSeconds(), totalText, sizeof(totalText));
      tft.fillRect(16, 242, 50, 22, themes.get().bg);
      tft.fillRect(414, 242, 50, 22, themes.get().bg);
      tft.fillRect(222, 204, 130, 24, themes.get().bg);
      tft.setTextSize(2);
      tft.setTextColor(themes.get().accent, themes.get().bg);
      tft.setCursor(16, 246); tft.print(elapsedText);
      tft.setCursor(414, 246); tft.print(totalText);
      tft.setTextColor(themes.get().text, themes.get().bg);
      tft.setCursor(222, 212); tft.printf("%s/%s", elapsedText, totalText);
      lastPlayerProgressTextPercent = percent;
    }
    tft.endWrite();

    lastPlayerProgressFill = fillw;
  }

  uint16_t vuColor(uint8_t level) const {
    return level >= 96 ? themes.get().error :
           (level >= 81 ? themes.get().vuHigh :
           (level >= 51 ? themes.get().vuMid : themes.get().vuLow));
  }

  void drawDirectVuSegments(int x, int y, int w,
                            uint8_t level, uint8_t peak,
                            uint8_t oldLevel, uint8_t oldPeak,
                            bool force) {
    const int h = 10;
    const int gap = (playerVuStyle == VU_THIN) ? 1 : 2;
    const int count = constrain((int)playerVuSegments,
                                (int)PLAYER_VU_SEGMENTS_MIN,
                                (int)PLAYER_VU_SEGMENTS_MAX);
    const int innerW = w - 2;
    const int segW = max(2, (innerW - gap * (count - 1)) / count);
    const int usedW = segW * count + gap * (count - 1);
    const int active = constrain((int)((level * count + 99) / 100), 0, count);
    const int oldActive = oldLevel == 255 ? -1 :
                          constrain((int)((oldLevel * count + 99) / 100), 0, count);
    const int peakSeg = peak == 0 ? -1 :
                        constrain((int)((peak * count - 1) / 100), 0, count - 1);
    const int oldPeakSeg = (oldPeak == 255 || oldPeak == 0) ? -1 :
                           constrain((int)((oldPeak * count - 1) / 100), 0, count - 1);

    auto segColor = [this, count](int i) -> uint16_t {
      const int pct = ((i + 1) * 100) / count;
      return pct > 95 ? themes.get().error :
             (pct > 80 ? themes.get().vuHigh :
             (pct > 50 ? themes.get().vuMid : themes.get().vuLow));
    };
    auto drawSegment = [&](int i, bool on) {
      const int sx = x + 1 + i * (segW + gap);
      const uint16_t color = on ? segColor(i) : themes.get().bg;
      tft.fillRect(sx, y + 1, segW, h - 2, themes.get().bg);
      if (!on) return;
      if (playerVuStyle == VU_DOT) {
        const int radius = max(1, min(segW, h - 2) / 2);
        tft.fillCircle(sx + segW / 2, y + h / 2, radius, color);
      } else if (playerVuStyle == VU_THIN) {
        const int thinW = max(1, segW / 2);
        tft.fillRect(sx + (segW - thinW) / 2, y + 1, thinW, h - 2, color);
      } else if (playerVuStyle == VU_LINE) {
        // v9.4-alpha4: fastest Hi-Fi line style — one vertical LCD primitive per active segment.
        const int lineX = sx + segW / 2;
        tft.drawFastVLine(lineX, y + 1, h - 2, color);
      } else {
        tft.fillRect(sx, y + 1, segW, h - 2, color);
      }
    };

    if (force || oldActive < 0) {
      tft.fillRect(x + 1, y + 1, usedW, h - 2, themes.get().bg);
      for (int i = 0; i < active; ++i) drawSegment(i, true);
    } else if (active > oldActive) {
      for (int i = oldActive; i < active; ++i) drawSegment(i, true);
    } else if (active < oldActive) {
      for (int i = active; i < oldActive; ++i) drawSegment(i, false);
    }

    if (oldPeakSeg >= 0 && oldPeakSeg != peakSeg) {
      drawSegment(oldPeakSeg, oldPeakSeg < active);
    }
    if (playerVuPeakEnabled && peakSeg >= 0) {
      const int px = x + 1 + peakSeg * (segW + gap);
      tft.drawRect(px, y + 1, segW, h - 2, themes.get().peak);
    }
  }

  void drawDirectVu(uint8_t l, uint8_t r, uint8_t peakL, uint8_t peakR, bool force = false) {
    if (!rt.audioPlaying && !rt.rtAudioTaskRunning) { l = r = 0; }
    const PlayerLayout& layout = iq200PlayerLayout(playerLayoutMode);
    tft.startWrite();
    drawDirectVuSegments(layout.vuLeft.x, layout.vuLeft.y, layout.vuLeft.w, l, peakL, lastPlayerVuL, lastPlayerPeakL, force);
    drawDirectVuSegments(layout.vuRight.x, layout.vuRight.y, layout.vuRight.w, r, peakR, lastPlayerVuR, lastPlayerPeakR, force);
    tft.endWrite();
  }

  void updatePeakHold(uint8_t l, uint8_t r, uint32_t now) {
    const uint32_t PEAK_HOLD_MS = playerVuPeakHoldMs;
    const uint8_t PEAK_FALL_PER_SEC = (uint8_t)(playerVuDecay * 8);
    uint32_t elapsed = playerPeakLastMs ? (now - playerPeakLastMs) : 0;
    playerPeakLastMs = now;

    if (!rt.audioPlaying && !rt.rtAudioTaskRunning) {
      playerPeakL = playerPeakR = 0;
      playerPeakHoldUntilL = playerPeakHoldUntilR = 0;
      return;
    }

    if (l >= playerPeakL) {
      playerPeakL = l;
      playerPeakHoldUntilL = now + PEAK_HOLD_MS;
    } else if ((int32_t)(now - playerPeakHoldUntilL) >= 0 && elapsed) {
      uint32_t fall = (elapsed * PEAK_FALL_PER_SEC + 999) / 1000;
      if (fall == 0) fall = 1;
      playerPeakL = fall >= playerPeakL ? 0 : (uint8_t)(playerPeakL - fall);
      if (playerPeakL < l) playerPeakL = l;
    }

    if (r >= playerPeakR) {
      playerPeakR = r;
      playerPeakHoldUntilR = now + PEAK_HOLD_MS;
    } else if ((int32_t)(now - playerPeakHoldUntilR) >= 0 && elapsed) {
      uint32_t fall = (elapsed * PEAK_FALL_PER_SEC + 999) / 1000;
      if (fall == 0) fall = 1;
      playerPeakR = fall >= playerPeakR ? 0 : (uint8_t)(playerPeakR - fall);
      if (playerPeakR < r) playerPeakR = r;
    }
  }

  void drawCdDiscPlaceholder(int x, int y, int w, int h) {
    tft.fillRect(x, y, w, h, themes.get().bg);
    tft.drawRect(x, y, w, h, themes.get().border);
    const int cx = x + w / 2;
    const int cy = y + h / 2 - 5;
    const int radius = min(w, h) / 3;
    tft.drawCircle(cx, cy, radius, themes.get().dim);
    tft.drawCircle(cx, cy, radius - 3, themes.get().border);
    tft.fillCircle(cx, cy, 5, themes.get().accent);
    tft.drawCircle(cx, cy, 10, themes.get().text);
    tft.setTextSize(1);
    tft.setTextColor(themes.get().dim, themes.get().bg);
    tft.setCursor(x + 20, y + h - 15);
    tft.print("CD DISC");
  }

  static bool jpegDimensions(const uint8_t* data, size_t size, int& width, int& height) {
    width = height = 0;
    if (!data || size < 4 || data[0] != 0xFF || data[1] != 0xD8) return false;
    size_t pos = 2;
    while (pos + 9 < size) {
      while (pos < size && data[pos] != 0xFF) ++pos;
      while (pos < size && data[pos] == 0xFF) ++pos;
      if (pos >= size) break;
      const uint8_t marker = data[pos++];
      if (marker == 0xD8 || marker == 0xD9 || marker == 0x01 || (marker >= 0xD0 && marker <= 0xD7)) continue;
      if (pos + 1 >= size) break;
      const uint16_t len = (uint16_t(data[pos]) << 8) | data[pos + 1];
      if (len < 2 || pos + len > size) break;
      const bool sof = (marker >= 0xC0 && marker <= 0xC3) ||
                       (marker >= 0xC5 && marker <= 0xC7) ||
                       (marker >= 0xC9 && marker <= 0xCB) ||
                       (marker >= 0xCD && marker <= 0xCF);
      if (sof && len >= 7) {
        height = (int(data[pos + 3]) << 8) | data[pos + 4];
        width  = (int(data[pos + 5]) << 8) | data[pos + 6];
        return width > 0 && height > 0;
      }
      pos += len;
    }
    return false;
  }

  static bool pngDimensions(const uint8_t* data, size_t size, int& width, int& height) {
    width = height = 0;
    if (!data || size < 24 || data[0] != 0x89 || data[1] != 0x50 || data[2] != 0x4E || data[3] != 0x47) return false;
    width = (int(data[16]) << 24) | (int(data[17]) << 16) | (int(data[18]) << 8) | int(data[19]);
    height = (int(data[20]) << 24) | (int(data[21]) << 16) | (int(data[22]) << 8) | int(data[23]);
    return width > 0 && height > 0;
  }

  bool drawJpegFitRgb565(const uint8_t* data, size_t size,
                         int srcW, int srcH,
                         int X, int Y, int W, int H) {
    if (!data || !size || srcW <= 0 || srcH <= 0) return false;

    const float fit = min(float(W) / float(srcW), float(H) / float(srcH));
    const int outW = max(1, min(W, int(srcW * fit + 0.5f)));
    const int outH = max(1, min(H, int(srcH * fit + 0.5f)));
    const int outX = X + (W - outW) / 2;
    const int outY = Y + (H - outH) / 2;

    // LovyanGFX JPEG decoders accept discrete downscale factors. Try the
    // closest useful factor first, then progressively smaller fallbacks.
    const float scales[] = {1.0f, 0.5f, 0.25f, 0.125f};
    int first = 0;
    if (fit < 0.5f) first = 1;
    if (fit < 0.25f) first = 2;
    if (fit < 0.125f) first = 3;

    for (int si = first; si < 4; ++si) {
      const float decodeScale = scales[si];
      const int decW = max(1, int(srcW * decodeScale + 0.5f));
      const int decH = max(1, int(srcH * decodeScale + 0.5f));
      if ((uint64_t)decW * (uint64_t)decH > 1600000ULL) continue;

      LGFX_Sprite decoded(&tft);
      decoded.setColorDepth(16);
      decoded.setPsram(true);
      if (!decoded.createSprite(decW, decH)) {
        Serial.printf("[ART][RGB565] decode alloc failed %dx%d scale=%.3f\n", decW, decH, decodeScale);
        continue;
      }
      decoded.fillScreen(TFT_BLACK);
      const bool decodedOk = decoded.drawJpg(data, size, 0, 0, decW, decH,
                                             0, 0, decodeScale, decodeScale);
      if (!decodedOk) {
        Serial.printf("[ART][RGB565] JPEG decode failed scale=%.3f bytes=%u\n",
                      decodeScale, (unsigned)size);
        decoded.deleteSprite();
        continue;
      }

      LGFX_Sprite fitted(&tft);
      fitted.setColorDepth(16);
      fitted.setPsram(true);
      if (!fitted.createSprite(outW, outH)) {
        Serial.printf("[ART][RGB565] fit alloc failed %dx%d\n", outW, outH);
        decoded.deleteSprite();
        continue;
      }
      for (int dy = 0; dy < outH; ++dy) {
        const int sy = min(decH - 1, (dy * decH) / outH);
        for (int dx = 0; dx < outW; ++dx) {
          const int sx = min(decW - 1, (dx * decW) / outW);
          fitted.drawPixel(dx, dy, decoded.readPixel(sx, sy));
        }
        if ((dy & 7) == 0) delay(0);
      }
      fitted.pushSprite(outX, outY);
      Serial.printf("[ART][RGB565] src=%dx%d decoded=%dx%d out=%dx%d pos=%d,%d decode=%.3f fit=%.3f OK\n",
                    srcW, srcH, decW, decH, outW, outH, outX, outY, decodeScale, fit);
      fitted.deleteSprite();
      decoded.deleteSprite();
      return true;
    }
    return false;
  }

  void drawDirectArtwork(bool force = false) {
    ArtworkCache& art = ArtworkCache::instance();
    const uint32_t gen = art.generation();
    const uint32_t now = millis();
    if (!force && gen == lastArtworkGeneration) return;
    if (!force && gen == failedArtworkGeneration && (uint32_t)(now - lastArtworkRetryMs) < 2000U) return;

    const UIRect& ar = iq200PlayerLayout(playerLayoutMode).artwork;
    const int X = ar.x, Y = ar.y, W = ar.w, H = ar.h;

    ArtworkCache::ReadGuard guard(art);
    if (!guard.locked()) return;

    // Do not keep a long TFT transaction open while JPEG/PNG decoders allocate
    // sprites and push pixels. Nested transactions caused intermittent failures.
    tft.fillRect(X, Y, W, H, themes.get().bg);
    bool rendered = false;
    const bool ready = art.data() && art.size() &&
                       (art.state() == ArtworkCache::JPEG_READY || art.state() == ArtworkCache::PNG_READY);

    if (ready) {
      int srcW = 0, srcH = 0;
      bool dimensionsOk = false;
      if (art.state() == ArtworkCache::JPEG_READY) {
        dimensionsOk = jpegDimensions(art.data(), art.size(), srcW, srcH);
        if (dimensionsOk) rendered = drawJpegFitRgb565(art.data(), art.size(), srcW, srcH, X, Y, W, H);
        else Serial.printf("[ART][TFT] JPEG dimensions failed bytes=%u path=%s\n",
                           (unsigned)art.size(), art.imagePath().c_str());
      } else {
        dimensionsOk = pngDimensions(art.data(), art.size(), srcW, srcH);
        if (dimensionsOk) {
          float scale = min(float(W) / float(srcW), float(H) / float(srcH));
          scale = max(0.03125f, min(4.0f, scale));
          const int outW = max(1, int(srcW * scale + 0.5f));
          const int outH = max(1, int(srcH * scale + 0.5f));
          const int drawX = X + (W - outW) / 2;
          const int drawY = Y + (H - outH) / 2;
          rendered = tft.drawPng(art.data(), art.size(), drawX, drawY,
                                 W, H, 0, 0, scale, scale);
        } else {
          Serial.printf("[ART][TFT] PNG dimensions failed bytes=%u path=%s\n",
                        (unsigned)art.size(), art.imagePath().c_str());
        }
      }
      Serial.printf("[ART][FIT] src=%dx%d box=%dx%d state=%d ok=%d path=%s\n",
                    srcW, srcH, W, H, int(art.state()), rendered ? 1 : 0,
                    art.imagePath().c_str());
    }

    if (!rendered) drawCdDiscPlaceholder(X, Y, W, H);
    tft.drawRect(X, Y, W, H, themes.get().border);

    if (rendered || !ready) {
      lastArtworkGeneration = gen;
      failedArtworkGeneration = 0xffffffffUL;
    } else {
      failedArtworkGeneration = gen;
      lastArtworkRetryMs = now;
      Serial.printf("[ART][TFT] render deferred retry=2000ms gen=%lu path=%s\n",
                    (unsigned long)gen, art.imagePath().c_str());
    }
  }

  void drawDirectVolume() {
    vol = rt.volumePercent;
    if (vol < 0) vol = 0;
    if (vol > 100) vol = 100;
    tft.startWrite();
    tft.fillRect(414, 208, 50, 22, themes.get().bg);
    tft.setTextSize(2);
    tft.setTextColor(themes.get().accent, themes.get().bg);
    tft.setCursor(420, 212);
    tft.printf("%d", vol);
    tft.endWrite();
  }

  void drawDirectTitle(bool resetScroll = false) {
    String artist, album;
    playerPathParts(artist, album);
    const PlayerLayout& layout = iq200PlayerLayout(playerLayoutMode);
    const int boxX = layout.title.x;
    const int ty = layout.title.y;
    const int boxW = layout.title.w;
    const int th = layout.title.h;
    const int tx = boxX + 16;
    const int tw = boxW - 32;
    if (resetScroll) {
      playerTitleScrollX = 0;
      playerTitleTextW = iqTextWidthPx(playerTitle(), PLAYER_TITLE_SCALE);
      playerTitleScrollMs = millis();
      playerTitlePauseUntil = millis() + PLAYER_TITLE_PAUSE_MS;
      playerTitleScrollReturning = false;
    }
    tft.startWrite();
    tft.fillRect(boxX, ty, boxW, th, themes.get().bg);
    tft.fillTriangle(boxX + 3, ty + th/2, boxX + 11, ty + 8, boxX + 11, ty + th - 8, themes.get().accent);
    tft.fillTriangle(boxX + boxW - 3, ty + th/2, boxX + boxW - 11, ty + 8, boxX + boxW - 11, ty + th - 8, themes.get().accent);
    iqDrawTextClipped(tft, tx - playerTitleScrollX, ty + 6, playerTitle(),
                      themes.get().text, themes.get().bg, PLAYER_TITLE_SCALE,
                      tx, ty, tw, th);
    if (!resetScroll) { tft.endWrite(); return; }
    tft.fillRect(218, 98, 248, 68, themes.get().bg);
    tft.setTextSize(2);
    tft.setTextColor(themes.get().dim, themes.get().bg);
    tft.setCursor(222, 100); tft.print(artist.substring(0, 21));
    tft.setTextColor(themes.get().accent, themes.get().bg);
    tft.setCursor(222, 130); tft.print(album.substring(0, 21));
    tft.setTextSize(1);
    tft.setTextColor(themes.get().warn, themes.get().bg);
    tft.setCursor(222, 158);
    tft.printf("%s  %0.1f kHz  %u bit  %s", playerCodecName(),
               rt.mediaSampleRate ? rt.mediaSampleRate / 1000.0f : 0.0f,
               (unsigned)rt.mediaBits, rt.mediaChannels == 2 ? "Stereo" : "Mono");
    tft.endWrite();
  }

  bool updatePlayerTitleMarquee(uint32_t now) {
    const PlayerLayout& layout = iq200PlayerLayout(playerLayoutMode);
    const int titleViewportW = max(1, (int)layout.title.w - 32);
    const int overflow = max(0, playerTitleTextW - titleViewportW);
    // Audio owns the shared SPI budget. Marquee is cosmetic and must yield
    // whenever the decoder reports stress or an underrun/short write occurred.
    if (rt.audioHealth < 100 || rt.audioUnderruns != lastPlayerUnderruns ||
        rt.audioShortWrites != 0) {
      playerTitlePauseUntil = now + 1000;
      return false;
    }
    if (overflow <= 0 || now < playerTitlePauseUntil || now - playerTitleScrollMs < PLAYER_TITLE_STEP_MS) return false;
    playerTitleScrollMs = now;
    if (!playerTitleScrollReturning) {
      playerTitleScrollX += PLAYER_TITLE_STEP_PX;
      if (playerTitleScrollX >= overflow) {
        playerTitleScrollX = overflow;
        playerTitleScrollReturning = true;
        playerTitlePauseUntil = now + PLAYER_TITLE_PAUSE_MS;
      }
    } else {
      playerTitleScrollX -= PLAYER_TITLE_STEP_PX;
      if (playerTitleScrollX <= 0) {
        playerTitleScrollX = 0;
        playerTitleScrollReturning = false;
        playerTitlePauseUntil = now + PLAYER_TITLE_PAUSE_MS;
      }
    }
    drawDirectTitle(false);
    return true;
  }

  void drawDirectState() {
    tft.startWrite();
    // Update the centre transport glyph only; metadata remains static per track.
    tft.fillRect(215, 276, 50, 42, themes.get().bg);
    tft.drawCircle(240, 297, 20, themes.get().accent);
    tft.setTextSize(2);
    tft.setTextColor(themes.get().text, themes.get().bg);
    tft.setCursor(232, 290);
    tft.print(rt.audioPlaying ? "II" : ">");
    tft.endWrite();
  }

  void drawDirectHealthLine() {
    tft.startWrite();
    tft.fillRect(50, 268, 398, 12, themes.get().bg);
    tft.setTextSize(1);
    tft.setTextColor(themes.get().dim, themes.get().bg);
    tft.setCursor(52, 270);
    tft.printf("BUF:%u%%  UDR:%lu", (unsigned int)rt.audioHealth, (unsigned long)rt.audioUnderruns);
    tft.setCursor(260, 270);
    tft.printf("SW:%lu  I2S:%lu", (unsigned long)rt.audioShortWrites, (unsigned long)rt.audioLastChunkBytes);
    tft.endWrite();
  }

  void updatePlayerVolumeOnly() {
    if (screen != SCR_PLAYER || !fbReady) return;
    renderer.beginFrame();
    profileDraw(profVolume, [&]() { drawDirectVolume(); });
    rt.playerMetaDraws++;
    lastPlayerVol = rt.volumePercent;
    renderer.dirty();
    renderer.endFrame(false);
    rt.rendererFrames = renderer.frameCount;
    rt.dirtyFrames = renderer.dirtyFrames;
    rt.partialFrames = renderer.partialFrames;
    rt.fullFrames = renderer.fullFrames;
  }

  // v9.8-alpha36: VU settings from Web/console must repaint only the
  // two VU rows. Artwork, title, progress, controls and the framebuffer stay intact.
  void updatePlayerVuOnly(const char* reason = "settings") {
    if (screen != SCR_PLAYER || !fbReady) return;
    const uint32_t now = millis();
    uint8_t vuL = playerVuL();
    uint8_t vuR = playerVuR();
    updatePeakHold(vuL, vuR, now);

    renderer.beginFrame();
    profileDraw(profVu, [&]() { drawDirectVu(vuL, vuR, playerPeakL, playerPeakR, true); });
    rt.playerVuDraws++;
    lastPlayerVuL = vuL;
    lastPlayerVuR = vuR;
    lastPlayerPeakL = playerPeakL;
    lastPlayerPeakR = playerPeakR;
    lastVuMs = now;
    renderer.dirty();
    renderer.endFrame(false);
    rt.rendererFrames = renderer.frameCount;
    rt.dirtyFrames = renderer.dirtyFrames;
    rt.partialFrames = renderer.partialFrames;
    rt.fullFrames = renderer.fullFrames;
    Serial.printf("[UI] partial redraw=VU reason=%s\n", reason ? reason : "settings");
  }

  void updatePlayerPartials(bool force = false) {
    if (screen != SCR_PLAYER || !fbReady) return;
    uint32_t now = millis();
    if (!force && now - lastPartialMs < PLAYER_FRAME_MS) return;
    lastPartialMs = now;

    int percent = playerProgress();
    bool any = false;

    if (force || now - lastProgressMs >= PLAYER_PROGRESS_MS) {
      lastProgressMs = now;
      rt.playerProgressTicks++;
      if (force || percent != lastPlayerPercent) {
        profileDraw(profProgress, [&]() { drawDirectProgress(percent); });
        rt.playerProgressDraws++;
        lastPlayerPercent = percent;
        any = true;
      }
    }
    if (force || vol != lastPlayerVol) {
      profileDraw(profVolume, [&]() { drawDirectVolume(); });
      rt.playerMetaDraws++;
      lastPlayerVol = vol;
      any = true;
    }
    if (force || now - lastVuMs >= playerVuIntervalMs()) {
      lastVuMs = now;
      rt.playerVuTicks++;
      uint8_t vuL = playerVuL();
      uint8_t vuR = playerVuR();
      updatePeakHold(vuL, vuR, now);
      const bool vuChanged = force || lastPlayerVuL == 255 || lastPlayerVuR == 255 ||
          abs((int)vuL - (int)lastPlayerVuL) >= PLAYER_VU_DELTA ||
          abs((int)vuR - (int)lastPlayerVuR) >= PLAYER_VU_DELTA;
      const bool peakChanged = force || playerPeakL != lastPlayerPeakL ||
          playerPeakR != lastPlayerPeakR;
      if (vuChanged || peakChanged) {
        profileDraw(profVu, [&]() { drawDirectVu(vuL, vuR, playerPeakL, playerPeakR, force); });
        rt.playerVuDraws++;
        lastPlayerVuL = vuL;
        lastPlayerVuR = vuR;
        lastPlayerPeakL = playerPeakL;
        lastPlayerPeakR = playerPeakR;
        any = true;
      }
    }
    if (!force && updatePlayerTitleMarquee(now)) {
      rt.playerMetaDraws++;
      any = true;
    }
    if (force || rt.audioPlaying != lastPlayerPlaying || rt.mediaState != lastPlayerState) {
      profileDraw(profState, [&]() { drawDirectState(); });
      rt.playerMetaDraws++;
      lastPlayerPlaying = rt.audioPlaying;
      lastPlayerState = rt.mediaState;
      any = true;
    }
    // v9.8-alpha22: artwork is generation-driven only. A forced refresh of
    // dynamic widgets (STOP/state/progress/VU) must never repaint the album art.
    // Entering Player still draws it because resetPlayerPartialCache() sets
    // lastArtworkGeneration to an impossible value.
    // v9.8-alpha38: JPEG/PNG decode and RGB565 scaling are forbidden while
    // the RT decoder is active. Large artwork work on Core1/SPI can starve the
    // SD prefetch path and cause audible stutter. The generation remains
    // pending and is rendered automatically as soon as playback becomes idle.
    const bool artworkSafeWindow = !rt.audioPlaying && !rt.rtAudioTaskRunning;
    if (artworkSafeWindow && ArtworkCache::instance().generation() != lastArtworkGeneration) {
      profileDraw(profArtwork, [&]() { drawDirectArtwork(false); });
      rt.artworkRenderedGeneration = lastArtworkGeneration;
      rt.playerMetaDraws++;
      any = true;
    }
    if (force || strncmp(lastPlayerTitle, playerTitle(), sizeof(lastPlayerTitle)) != 0) {
      profileDraw(profTitle, [&]() { drawDirectTitle(true); });
      rt.playerMetaDraws++;
      strncpy(lastPlayerTitle, playerTitle(), sizeof(lastPlayerTitle)-1);
      lastPlayerTitle[sizeof(lastPlayerTitle)-1] = 0;
      any = true;
    }
    // Maximum-speed player: no periodic health/status LCD writes during playback.
    if (force) {
      lastHealthMs = now;
      lastPlayerBytes = rt.mediaPlayedBytes ? rt.mediaPlayedBytes : rt.wavPlayedBytes;
      lastPlayerUnderruns = rt.audioUnderruns;
    }
    if (any) {
      renderer.dirty();
      renderer.endFrame(false);
      rt.rendererFrames = renderer.frameCount;
      rt.dirtyFrames = renderer.dirtyFrames;
      rt.partialFrames = renderer.partialFrames;
      rt.fullFrames = renderer.fullFrames;
    }
  }

  void playerScreen() {
    screen = SCR_PLAYER;
    resetPlayerPartialCache();
    appOpened(APP_PLAYER);
    vol = rt.volumePercent;
    if (vol < 0) vol = 0;
    if (vol > 100) vol = 100;
    if (!rt.audioPlaying) audio.begin();
    audio.setVolume(vol);

    const int percent = playerProgress();
    header("ПЛЕЄР 2.0");

    // Static, minimal surface: one full framebuffer push only on entering Player.
    const PlayerLayout& layout = iq200PlayerLayout(playerLayoutMode);
    fb.fillRect(layout.content.x, layout.content.y, layout.content.w, layout.content.h, themes.get().bg);
    fb.drawFastHLine(20, layout.separatorY, 440, themes.get().progress);

    String artist, album;
    playerPathParts(artist, album);

    fb.fillTriangle(layout.title.x + 3, layout.title.y + layout.title.h/2,
                    layout.title.x + 11, layout.title.y + 8,
                    layout.title.x + 11, layout.title.y + layout.title.h - 8, themes.get().accent);
    fb.fillTriangle(layout.title.x + layout.title.w - 3, layout.title.y + layout.title.h/2,
                    layout.title.x + layout.title.w - 11, layout.title.y + 8,
                    layout.title.x + layout.title.w - 11, layout.title.y + layout.title.h - 8, themes.get().accent);
    iqDrawTextClipped(fb, layout.title.x + 16, layout.title.y + 6, playerTitle(),
                      themes.get().text, themes.get().bg, PLAYER_TITLE_SCALE,
                      layout.title.x + 16, layout.title.y, layout.title.w - 32, layout.title.h);
    playerTitleTextW = iqTextWidthPx(playerTitle(), PLAYER_TITLE_SCALE);
    playerTitleScrollX = 0;
    playerTitleScrollMs = millis();
    playerTitlePauseUntil = millis() + PLAYER_TITLE_PAUSE_MS;
    playerTitleScrollReturning = false;

    // Artwork Focus metadata coordinates from the approved 480x320 mockup.
    fb.setTextSize(2);
    fb.setTextColor(themes.get().dim, themes.get().bg);
    fb.setCursor(222, 100); fb.print(artist.substring(0, 21));
    fb.setTextColor(themes.get().accent, themes.get().bg);
    fb.setCursor(222, 130); fb.print(album.substring(0, 21));

    fb.setTextSize(1);
    fb.setTextColor(themes.get().warn, themes.get().bg);
    fb.setCursor(222, 158);
    fb.printf("%s  %0.1f kHz  %u bit  %s", playerCodecName(),
              rt.mediaSampleRate ? rt.mediaSampleRate / 1000.0f : 0.0f,
              (unsigned)rt.mediaBits, rt.mediaChannels == 2 ? "Stereo" : "Mono");

    fb.setTextColor(themes.get().border, themes.get().bg);
    fb.setCursor(max(2, layout.vuLeft.x - 12), layout.vuLeft.y + 2); fb.print("L");
    fb.setCursor(max(2, layout.vuRight.x - 12), layout.vuRight.y + 2); fb.print("R");

    // Album artwork area. JPEG is pushed directly after the framebuffer.
    fb.drawRect(layout.artwork.x, layout.artwork.y, layout.artwork.w, layout.artwork.h, themes.get().border);
    fb.setTextSize(1);
    fb.setTextColor(themes.get().border, themes.get().bg);
    fb.setCursor(layout.artwork.x + 30, layout.artwork.y + 43); fb.print("CD DISC");

    // Segmented VU placeholders. Segment count is runtime-configurable.
    fb.drawRect(layout.vuLeft.x, layout.vuLeft.y, layout.vuLeft.w, layout.vuLeft.h, themes.get().border);
    fb.drawRect(layout.vuRight.x, layout.vuRight.y, layout.vuRight.w, layout.vuRight.h, themes.get().border);

    char elapsedText[12], totalText[12];
    formatPlayerTime(playerElapsedSeconds(), elapsedText, sizeof(elapsedText));
    formatPlayerTime(playerDurationSeconds(), totalText, sizeof(totalText));

    fb.setTextSize(2);
    fb.setTextColor(themes.get().text, themes.get().bg);
    fb.setCursor(222, 212); fb.printf("%s / %s", elapsedText, totalText);
    fb.setTextColor(themes.get().warn, themes.get().bg);
    fb.setCursor(356, 212); fb.print(rt.queueRepeatMode == 1 ? "ONE" : (rt.queueRepeatMode == 2 ? "ALL" : "OFF"));
    fb.setTextColor(themes.get().accent, themes.get().bg);
    fb.setCursor(420, 212); fb.printf("%d", vol);

    fb.drawRect(layout.progress.x, layout.progress.y, layout.progress.w, layout.progress.h, themes.get().border);
    int fillw = (layout.progress.w - 4) * percent / 100;
    if (fillw > 0) fb.fillRect(layout.progress.x + 2, layout.progress.y + 2, fillw, layout.progress.h - 4, themes.get().progress);
    fb.setTextSize(2);
    fb.setTextColor(themes.get().accent, themes.get().bg);
    fb.setCursor(16, 246); fb.print(elapsedText);
    fb.setCursor(414, 246); fb.print(totalText);

    // Five compact transport glyphs, matching the approved mockup.
    const int cy = 297;
    const int cx[5] = {48, 144, 240, 336, 432};
    const char* glyph[5] = {"|<", "<<", rt.audioPlaying ? "II" : ">", ">>", ">|"};
    for (int i = 0; i < 5; ++i) {
      const uint16_t c = (i == 2) ? themes.get().accent : themes.get().border;
      fb.drawCircle(cx[i], cy, i == 2 ? 20 : 17, c);
      fb.setTextSize(i == 2 ? 2 : 1);
      fb.setTextColor(themes.get().text, themes.get().bg);
      fb.setCursor(cx[i] - (i == 2 ? 8 : 6), cy - (i == 2 ? 7 : 4));
      fb.print(glyph[i]);
    }

    // Player 2.0 uses the full lower 38 px for controls; no generic footer here.
    present();
    updatePlayerPartials(true);
  }

  void settings() {
    appOpened(APP_SETTINGS);
    screen = SCR_SETTINGS;
    header("НАЛАШТУВАННЯ");
    text(30, 62, "IQ200 OS v9.1.1 Player UI", themes.get().accent, themes.get().bg, 3);
    fb.setTextSize(2);
    fb.setTextColor(themes.get().text, themes.get().bg);
    fb.setCursor(30, 125); fb.print("Display: ILI9488");
    fb.setCursor(30, 155); fb.print("SPI TFT: 40 MHz");
    fb.setCursor(30, 185); fb.print("PSRAM: qio_opi");
    fb.setCursor(30, 215); fb.print("Radio: disabled");
    fb.setCursor(30, 230); fb.print("CoreOS: READY");
    fb.setCursor(250, 230); fb.print("v9.1.1");
    fb.setCursor(30, 245); fb.printf("App: %s", rt.currentApp);
    fb.setCursor(250, 245); fb.printf("W:%d", windows.depth());
    footer("OK - назад");
    present();
  }


  void healthScreen() {
    appOpened(APP_HEALTH);
    screen = SCR_STATUS;
    header("HEALTH");
    text(25, 58, "Стабільність", themes.get().accent, themes.get().bg, 3);

    fb.setTextSize(2);
    fb.setTextColor(rt.core0Ok ? themes.get().ok : themes.get().error, themes.get().bg);
    fb.setCursor(25, 115);
    fb.printf("Core0: %s %lums", rt.core0Ok ? "OK" : "BAD", (unsigned long)rt.core0AgeMs);

    fb.setTextColor(rt.core1Ok ? themes.get().ok : themes.get().error, themes.get().bg);
    fb.setCursor(25, 145);
    fb.printf("Core1: %s %lums", rt.core1Ok ? "OK" : "BAD", (unsigned long)rt.core1AgeMs);

    fb.setTextColor(themes.get().text, themes.get().bg);
    fb.setCursor(25, 175);
    fb.printf("Queue drops: %lu", (unsigned long)rt.eventQueueDrops);

    fb.setCursor(25, 205);
    fb.printf("Heap: %lu", (unsigned long)ESP.getFreeHeap());

    fb.setCursor(25, 235);
    fb.printf("PSRAM: %luK", (unsigned long)sys.psramFreeKB());

    footer("OK - назад");
    present();
  }


  void schedulerScreen() {
    appOpened(APP_SCHEDULER);
    screen = SCR_STATUS;
    header("SCHED");

    text(22, 55, "Core0 services", themes.get().accent, themes.get().bg, 2);
    fb.setTextSize(2);
    fb.setTextColor(themes.get().text, themes.get().bg);
    fb.setCursor(25, 95); fb.printf("Audio: %lu", (unsigned long)rt.audioTicks);
    fb.setCursor(25, 125); fb.printf("SD   : %lu", (unsigned long)rt.sdTicks);
    fb.setCursor(25, 155); fb.printf("WiFi : %lu", (unsigned long)rt.wifiTicks);

    text(250, 55, "Core1 GUI", themes.get().accent, themes.get().bg, 2);
    fb.setCursor(250, 95); fb.printf("GUI  : %lu", (unsigned long)rt.guiTicks);
    fb.setCursor(250, 125); fb.printf("Input: %lu", (unsigned long)rt.inputTicks);
    fb.setCursor(250, 155); fb.printf("Anim : %lu", (unsigned long)rt.animationTicks);

    fb.setCursor(25, 220);
    fb.printf("Mode: Queue + Scheduler");

    footer("OK - назад");
    present();
  }


  void appOpened(IQAppId app) {
    apps.open(app);
    windows.push(app);
    notifications.notify(apps.name(app));
    rt.appSwitches = apps.switchCount();
    strncpy(rt.currentApp, apps.name(app), sizeof(rt.currentApp) - 1);
    rt.currentApp[sizeof(rt.currentApp) - 1] = 0;
  }


  void appManagerScreen() {
    screen = SCR_STATUS;
    header("APPS");
    text(25, 58, "App Manager", themes.get().accent, themes.get().bg, 3);
    fb.setTextSize(2);
    fb.setTextColor(themes.get().text, themes.get().bg);
    fb.setCursor(25, 115); fb.printf("Current: %s", rt.currentApp);
    fb.setCursor(25, 145); fb.printf("Switches: %lu", (unsigned long)rt.appSwitches);
    fb.setCursor(25, 175); fb.print("Core1 owns UI apps");
    fb.setCursor(25, 205); fb.print("Core0 owns services");
    footer("OK - назад");
    present();
  }


  void windowManagerScreen() {
    screen = SCR_STATUS;
    header("WINDOWS");
    text(25, 58, "Window Manager", themes.get().accent, themes.get().bg, 3);

    fb.setTextSize(2);
    fb.setTextColor(themes.get().text, themes.get().bg);
    fb.setCursor(25, 115); fb.printf("Current: %s", rt.currentApp);
    fb.setCursor(25, 145); fb.printf("Depth: %d", windows.depth());
    fb.setCursor(25, 175); fb.printf("Can back: %s", windows.canBack() ? "YES" : "NO");
    fb.setCursor(25, 205); fb.printf("Notify: %s", notifications.message());

    footer("OK - назад");
    present();
  }


  void coreOsScreen() {
    screen = SCR_STATUS;
    header("CORE OS");
    text(25, 58, "IQ200 CoreOS", themes.get().accent, themes.get().bg, 3);

    fb.setTextSize(2);
    fb.setTextColor(themes.get().text, themes.get().bg);
    fb.setCursor(25, 115); fb.print("Version: 9.0.13");
    fb.setCursor(25, 145); fb.print("State: READY");
    fb.setCursor(25, 175); fb.print("Core0: services");
    fb.setCursor(25, 205); fb.print("Core1: GUI");
    fb.setCursor(25, 235); fb.print("Broker: EventQueue");

    footer("OK - назад");
    present();
  }

  void otaScreen() {
    screen = SCR_STATUS;
    header("OTA");
    text(25, 58, "OTA Service", themes.get().accent, themes.get().bg, 3);

    fb.setTextSize(2);
    fb.setTextColor(themes.get().warn, themes.get().bg);
    fb.setCursor(25, 125); fb.print("Status: DISABLED");
    fb.setTextColor(themes.get().text, themes.get().bg);
    fb.setCursor(25, 165); fb.print("WiFi required");
    fb.setCursor(25, 195); fb.print("Future: web update");

    footer("OK - назад");
    present();
  }


  void servicesScreen() {
    screen = SCR_STATUS;
    header("SERVICES");
    text(25, 58, "Service Manager", themes.get().accent, themes.get().bg, 3);

    fb.setTextSize(2);
    fb.setTextColor(themes.get().text, themes.get().bg);
    fb.setCursor(25, 115); fb.print("Audio  : ON");
    fb.setCursor(25, 145); fb.print("Storage: ON");
    fb.setCursor(25, 175); fb.print("WiFi   : ON");
    fb.setCursor(25, 205); fb.print("OTA    : OFF");
    fb.setCursor(25, 235); fb.printf("Ticks  : %lu", (unsigned long)rt.serviceTicks);

    footer("OK - назад");
    present();
  }

  void renderScreen() {
    screen = SCR_STATUS;
    header("RENDER");
    text(25, 58, "Renderer v1", themes.get().accent, themes.get().bg, 3);

    fb.setTextSize(2);
    fb.setTextColor(themes.get().text, themes.get().bg);
    fb.setCursor(25, 115); fb.printf("Frames: %lu", (unsigned long)rt.rendererFrames);
    fb.setCursor(25, 145); fb.printf("FPS   : %u / %u", fps, UI_TARGET_FPS);
    fb.setCursor(25, 175); fb.print("Mode  : PSRAM FB");
    fb.setCursor(25, 205); fb.print("SPI   : 40 MHz");

    widgets.progress(fb, 25, 240, 260, 20, fps > 60 ? 100 : fps * 100 / 60, themes.get().ok, themes.get().bg);

    footer("OK - назад");
    present();
  }



  void diagScreen() {
    screen = SCR_STATUS;
    header("DIAGNOSTICS");
    text(25, 50, "Core Stabilization", themes.get().accent, themes.get().bg, 2);

    fb.setTextSize(1);
    fb.setTextColor(themes.get().text, themes.get().bg);
    fb.setCursor(25, 86);  fb.printf("Heap C0:%lu C1:%lu", (unsigned long)rt.core0HeapFree, (unsigned long)rt.core1HeapFree);
    fb.setCursor(25, 106); fb.printf("Stack C0:%lu C1:%lu", (unsigned long)rt.core0StackHighWater, (unsigned long)rt.core1StackHighWater);
    fb.setCursor(25, 126); fb.printf("Audio underruns:%lu short:%lu health:%u", (unsigned long)rt.audioUnderruns, (unsigned long)rt.audioShortWrites, (unsigned int)rt.audioHealth);
    fb.setCursor(25, 146); fb.printf("DB tracks:%d volumes:%d art:%d", rt.dbTrackCount, rt.dbVolumeCount, rt.dbArtCount);
    fb.setCursor(25, 166); fb.printf("Pipe F:%lu/%lu M:%lu/%lu D:%lu/%lu",
      (unsigned long)rt.pipelineFileQueueDepth, (unsigned long)rt.pipelineFileQueueSize,
      (unsigned long)rt.pipelineMetaQueueDepth, (unsigned long)rt.pipelineMetaQueueSize,
      (unsigned long)rt.pipelineDbQueueDepth, (unsigned long)rt.pipelineDbQueueSize);
    fb.setCursor(25, 186); fb.printf("Events:%lu drops:%lu busDrops:%lu", (unsigned long)rt.eventBusPosts, (unsigned long)rt.eventQueueDrops, (unsigned long)rt.eventBusDrops);
    fb.setCursor(25, 206); fb.printf("Scan async:%s lock:%s progress:%u", rt.asyncScanPolicy ? "ON" : "OFF", rt.scanLock ? "ON" : "OFF", rt.scanProgress);
    fb.setCursor(25, 226); fb.printf("Partial render:%s full:%lu partial:%lu", rt.partialRenderPolicy ? "ON" : "OFF", (unsigned long)rt.fullFrames, (unsigned long)rt.partialFrames);
    fb.setCursor(25, 246); fb.printf("SD latency:%lu/%lu ms FPS:%u/%u", (unsigned long)rt.sdLatencyLastMs, (unsigned long)rt.sdLatencyMaxMs, fps, UI_TARGET_FPS);
    fb.setCursor(25, 266); fb.printf("Pipe RB:%u%% cache:%u%% rd:%lu", rt.playerRingFillPct, rt.playerCacheHitPct, (unsigned long)rt.playerReadAheadBytes);

    Serial.println("[DIAGPRO] ===== IQ200 v9.1.1 Next Track Engine Diagnostics =====");
    Serial.printf("[DIAGPRO] CPU loops C0=%lu C1=%lu age C0=%lums C1=%lums\n", (unsigned long)rt.core0Loops, (unsigned long)rt.core1Loops, (unsigned long)rt.core0AgeMs, (unsigned long)rt.core1AgeMs);
    Serial.printf("[DIAGPRO] Heap C0=%lu C1=%lu free=%lu PSRAM=%lu freePSRAM=%lu\n", (unsigned long)rt.core0HeapFree, (unsigned long)rt.core1HeapFree, (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getPsramSize(), (unsigned long)ESP.getFreePsram());
    Serial.printf("[DIAGPRO] Stack C0=%lu C1=%lu Audio=%lu\n", (unsigned long)rt.core0StackHighWater, (unsigned long)rt.core1StackHighWater, (unsigned long)rt.audioTaskStackHighWater);
    Serial.printf("[DIAGPRO] SD ok=%d MB=%llu latency last/max=%lu/%lu ms\n", rt.sdOk ? 1 : 0, (unsigned long long)rt.sdMB, (unsigned long)rt.sdLatencyLastMs, (unsigned long)rt.sdLatencyMaxMs);
    Serial.printf("[DIAGPRO] FPS=%u target=%u frames=%lu full=%lu partial=%lu dirty=%lu\n", fps, UI_TARGET_FPS, (unsigned long)rt.rendererFrames, (unsigned long)rt.fullFrames, (unsigned long)rt.partialFrames, (unsigned long)rt.dirtyFrames);
    Serial.printf("[DIAGPRO] Audio underruns=%lu shortWrites=%lu health=%u rtLoops=%lu chunk=%lu\n", (unsigned long)rt.audioUnderruns, (unsigned long)rt.audioShortWrites, (unsigned int)rt.audioHealth, (unsigned long)rt.audioRtLoops, (unsigned long)rt.audioLastChunkBytes);
    Serial.printf("[DIAGPRO] Player state=%s ring=%u%% cache=%u%% hits=%lu miss=%lu readAhead=%lu decoderLoad=%u%% sdRetry=%lu sdErr=%lu\n",
      rt.playerStateName, rt.playerRingFillPct, rt.playerCacheHitPct,
      (unsigned long)rt.playerCacheHits, (unsigned long)rt.playerCacheMisses,
      (unsigned long)rt.playerReadAheadBytes, rt.playerDecoderLoadPct,
      (unsigned long)rt.playerSdRetries, (unsigned long)rt.playerSdErrors);
    Serial.printf("[DIAGPRO] Events posts=%lu drops=%lu busDrops=%lu media=%lu queue=%lu db=%lu scan=%lu\n", (unsigned long)rt.eventBusPosts, (unsigned long)rt.eventQueueDrops, (unsigned long)rt.eventBusDrops, (unsigned long)rt.mediaEventCount, (unsigned long)rt.queueEventCount, (unsigned long)rt.dbEventCount, (unsigned long)rt.scanEventCount);
    Serial.printf("[DIAGPRO] DB tracks=%d mp3=%d flac=%d wav=%d volumes=%d art=%d scan=%u%% elapsed=%lums\n", rt.dbTrackCount, rt.dbMp3Count, rt.dbFlacCount, rt.dbWavCount, rt.dbVolumeCount, rt.dbArtCount, rt.scanProgress, (unsigned long)rt.scanElapsedMs);
    Serial.printf("[DIAGPRO] Stability status=%s burnin=%d uptime=%lus recoveries=%lu sdErr=%lu leakWarn=%lu\n", rt.stabilityStatus, rt.burninActive ? 1 : 0, (unsigned long)(rt.stabilityUptimeMs/1000UL), (unsigned long)rt.stabilityRecoveries, (unsigned long)rt.stabilitySdErrors, (unsigned long)rt.stabilityLeakWarnings);

    rt.diagLastShownMs = millis();
    footer("diag | v9.1.1 Next Track Engine");
    present();
  }


  void scanLockScreen() {
    screen = SCR_STATUS;
    header("SD SCAN");

    uint32_t elapsedMs = rt.scanLock ? (millis() - rt.scanStartMs) : rt.scanElapsedMs;
    rt.scanElapsedMs = elapsedMs;
    uint32_t sec = elapsedMs / 1000;
    uint32_t hh = sec / 3600;
    uint32_t mm = (sec / 60) % 60;
    uint32_t ss = sec % 60;
    uint32_t speed = sec ? (uint32_t)(rt.scanFiles / sec) : 0;
    uint32_t eta = 0;
    if (rt.scanProgress > 1 && rt.scanProgress < 100) {
      eta = (elapsedMs / 1000UL) * (100UL - rt.scanProgress) / rt.scanProgress;
    }

    text(25, 50, "Сканування SD", themes.get().warn, themes.get().bg, 3);

    fb.setTextSize(2);
    fb.setTextColor(themes.get().accent, themes.get().bg);
    fb.setCursor(25, 88);
    fb.printf("Time %02lu:%02lu:%02lu", (unsigned long)hh, (unsigned long)mm, (unsigned long)ss);

    fb.setTextColor(themes.get().text, themes.get().bg);
    fb.setCursor(25, 120); fb.printf("Files : %d", rt.scanFiles);
    fb.setCursor(250, 120); fb.printf("Dirs: %d", rt.scanDirs);
    fb.setCursor(25, 148); fb.printf("Tracks: %d", rt.scanTracks);
    fb.setCursor(250, 148); fb.printf("%lu f/s", (unsigned long)speed);

    fb.setTextSize(1);
    fb.setTextColor(themes.get().dim, themes.get().bg);
    fb.setCursor(25, 182); fb.printf("MP3:%d  FLAC:%d  WAV:%d", rt.scanMp3, rt.scanFlac, rt.scanWav);
    fb.setCursor(25, 202);
    if (rt.scanProgress <= 1 || rt.scanProgress >= 100 || rt.scanFiles == 0) {
      fb.printf("ETA:--:--   Progress:%u%%", rt.scanProgress);
    } else {
      uint32_t eh = eta / 3600;
      uint32_t em = (eta / 60) % 60;
      uint32_t es = eta % 60;
      fb.printf("ETA:%02lu:%02lu:%02lu   Progress:%u%%", (unsigned long)eh, (unsigned long)em, (unsigned long)es, rt.scanProgress);
    }

    fb.setTextColor(themes.get().border, themes.get().bg);
    fb.setCursor(25, 222); fb.print(rt.scanMessage);
    fb.setCursor(25, 240);
    char pathBuf[62];
    strncpy(pathBuf, rt.scanCurrentPath, sizeof(pathBuf) - 1);
    pathBuf[sizeof(pathBuf) - 1] = 0;
    fb.print(pathBuf);

    widgets.progress(fb, 25, 262, 360, 20, rt.scanProgress, themes.get().accent, themes.get().bg);
    fb.setTextColor(themes.get().border, themes.get().bg);
    fb.setCursor(25, 292); fb.print("All media commands wait until scan finishes");
    present();
  }

  void libraryScreen() {
    screen = SCR_LIBRARY;
    fb.fillScreen(themes.get().bg);
    fb.fillRoundRect(10, 8, 460, 34, 8, themes.get().panel);
    text(22, 18, "MUSIC LIBRARY", themes.get().text, themes.get().panel, 2);
    fb.setTextSize(1);
    fb.setTextColor(themes.get().dim, themes.get().bg);
    fb.setCursor(300, 20);
    fb.printf("v9.1.1");

    const int tracks = rt.dbTrackCount > 0 ? rt.dbTrackCount : rt.playlistCount;
    const int mp3 = rt.dbMp3Count > 0 ? rt.dbMp3Count : rt.scanMp3;
    const int flac = rt.dbFlacCount > 0 ? rt.dbFlacCount : rt.scanFlac;
    const int wav = rt.dbWavCount > 0 ? rt.dbWavCount : rt.scanWav;
    const int art = rt.dbArtCount;

    fb.drawRoundRect(18, 54, 210, 184, 10, themes.get().border);
    fb.setTextSize(2);
    fb.setTextColor(themes.get().accent, themes.get().bg);
    fb.setCursor(34, 70);  fb.print("Artists");
    fb.setTextColor(themes.get().text, themes.get().bg);
    fb.setCursor(34, 98);  fb.print("Albums");
    fb.setCursor(34, 126); fb.print("Genres");
    fb.setCursor(34, 154); fb.print("Folders");
    fb.setCursor(34, 182); fb.print("Recent");
    fb.setCursor(34, 210); fb.print("Most");

    fb.fillRoundRect(238, 54, 224, 184, 10, 0x0841);
    fb.setTextSize(1);
    fb.setTextColor(themes.get().dim, 0x0841);
    fb.setCursor(252, 70);  fb.printf("Tracks : %d", tracks);
    fb.setCursor(252, 92);  fb.printf("FLAC   : %d", flac);
    fb.setCursor(252, 114); fb.printf("MP3    : %d", mp3);
    fb.setCursor(252, 136); fb.printf("WAV    : %d", wav);
    fb.setCursor(252, 158); fb.printf("Art    : %d", art);
    fb.setCursor(252, 180); fb.printf("Artists: %d", rt.libraryArtistCount);
    fb.setCursor(252, 198); fb.printf("Albums : %d", rt.libraryAlbumCount);
    fb.setCursor(252, 216); fb.printf("Source : media.idx");

    fb.drawRoundRect(18, 238, 444, 48, 8, themes.get().border);
    fb.setTextSize(1);
    fb.setTextColor(themes.get().text, themes.get().bg);
    fb.setCursor(30, 252);
    fb.print("Commands: artists albums genres folders recent most");
    fb.setCursor(30, 270);
    fb.print("libbuild creates artist/album/genre/folder indexes");

    footer("Build Hygiene v9.1.1");
    present();
  }


  void albumBrowserScreen() {
    screen = SCR_ALBUMS;
    fb.fillScreen(themes.get().bg);
    fb.fillRoundRect(10, 8, 460, 34, 8, themes.get().panel);
    text(22, 18, "ALBUM BROWSER", themes.get().text, themes.get().panel, 2);
    fb.setTextSize(1);
    fb.setTextColor(themes.get().dim, themes.get().panel);
    fb.setCursor(330, 20);
    fb.print("v9.1.1");

    const int tracks = rt.dbTrackCount > 0 ? rt.dbTrackCount : rt.playlistCount;
    const int flac = rt.dbFlacCount > 0 ? rt.dbFlacCount : rt.scanFlac;
    const int mp3 = rt.dbMp3Count > 0 ? rt.dbMp3Count : rt.scanMp3;
    const int wav = rt.dbWavCount > 0 ? rt.dbWavCount : rt.scanWav;
    const int art = rt.dbArtCount;

    fb.drawRoundRect(18, 54, 210, 184, 10, themes.get().border);
    fb.setTextSize(2);
    fb.setTextColor(themes.get().accent, themes.get().bg);
    fb.setCursor(34, 70);  fb.print("Albums");
    fb.setTextColor(themes.get().text, themes.get().bg);
    fb.setCursor(34, 100); fb.print("Artists");
    fb.setCursor(34, 130); fb.print("Folders");
    fb.setCursor(34, 160); fb.print("Search");
    fb.setCursor(34, 190); fb.print("Queue");

    fb.fillRoundRect(238, 54, 224, 184, 10, 0x0841);
    fb.setTextSize(1);
    fb.setTextColor(themes.get().text, 0x0841);
    fb.setCursor(252, 70);  fb.print("Album index foundation");
    fb.setTextColor(themes.get().dim, 0x0841);
    fb.setCursor(252, 96);  fb.printf("Tracks : %d", tracks);
    fb.setCursor(252, 116); fb.printf("FLAC   : %d", flac);
    fb.setCursor(252, 136); fb.printf("MP3    : %d", mp3);
    fb.setCursor(252, 156); fb.printf("WAV    : %d", wav);
    fb.setCursor(252, 176); fb.printf("Art    : %d", art);
    fb.setCursor(252, 206); fb.print("Source : media.idx");

    fb.drawRoundRect(18, 248, 444, 44, 8, themes.get().border);
    fb.setTextSize(1);
    fb.setTextColor(themes.get().text, themes.get().bg);
    fb.setCursor(30, 260);
    fb.print("Commands: albums/album, library/lib, find <album>");
    fb.setCursor(30, 278);
    fb.print("Next: real album.idx + cover cache + track list");

    footer("Album Browser UI v9.1.1");
    present();
  }


  void searchScreen() {
    screen = SCR_LIBRARY;
    fb.fillScreen(themes.get().bg);
    fb.fillRoundRect(10, 8, 460, 34, 8, themes.get().panel);
    text(22, 18, "SEARCH", themes.get().text, themes.get().panel, 2);
    fb.setTextSize(1);
    fb.setTextColor(themes.get().dim, themes.get().panel);
    fb.setCursor(360, 20);
    fb.print("v9.1.1");

    fb.drawRoundRect(18, 54, 444, 56, 10, themes.get().border);
    fb.setTextSize(1);
    fb.setTextColor(themes.get().dim, themes.get().bg);
    fb.setCursor(32, 68); fb.print("Last query:");
    fb.setTextSize(2);
    fb.setTextColor(themes.get().accent, themes.get().bg);
    fb.setCursor(32, 86);
    if (strlen(rt.dbFindQuery)) fb.print(rt.dbFindQuery);
    else fb.print("find <text>");

    fb.fillRoundRect(18, 122, 444, 86, 10, 0x0841);
    fb.setTextSize(1);
    fb.setTextColor(themes.get().text, 0x0841);
    fb.setCursor(32, 138); fb.printf("Results : %d", rt.dbFindResults);
    fb.setCursor(32, 160); fb.printf("Limit   : %d", rt.dbFindLimit);
    fb.setCursor(32, 182); fb.printf("Time    : %lu ms", (unsigned long)rt.dbFindLastMs);
    fb.setCursor(245, 138); fb.printf("Tracks : %d", rt.dbTrackCount > 0 ? rt.dbTrackCount : rt.playlistCount);
    fb.setCursor(245, 160); fb.printf("Source : media.idx");
    fb.setCursor(245, 182); fb.printf("DB Vol : %d", rt.dbVolumeCount);

    fb.drawRoundRect(18, 222, 444, 62, 8, themes.get().border);
    fb.setTextSize(1);
    fb.setTextColor(themes.get().text, themes.get().bg);
    fb.setCursor(30, 236); fb.print("Commands: find <text> / findall <text>");
    fb.setCursor(30, 254); fb.print("Results are printed to Serial; UI shows summary.");
    fb.setCursor(30, 272); fb.print("Next: on-screen encoder text input + result list.");

    footer("Search UI v9.1.1");
    present();
  }


  void favoritesScreen() {
    screen = SCR_FAVORITES;
    fb.fillScreen(themes.get().bg);
    fb.fillRoundRect(10, 8, 460, 34, 8, themes.get().panel);
    text(22, 18, "FAVORITES", themes.get().text, themes.get().panel, 2);
    fb.setTextSize(1);
    fb.setTextColor(themes.get().dim, themes.get().panel);
    fb.setCursor(350, 20);
    fb.print("v9.1.1");

    fb.drawRoundRect(18, 54, 210, 168, 10, themes.get().border);
    fb.setTextSize(2);
    fb.setTextColor(themes.get().warn, themes.get().bg);
    fb.setCursor(34, 72); fb.print("Favorites");
    fb.setTextColor(themes.get().text, themes.get().bg);
    fb.setCursor(34, 110); fb.printf("Count %d", rt.favoriteCount);
    fb.setTextSize(1);
    fb.setTextColor(themes.get().dim, themes.get().bg);
    fb.setCursor(34, 150); fb.print("SD-backed list");
    fb.setCursor(34, 170); fb.print("/iq200/db/favorites/favorites.db");

    fb.fillRoundRect(238, 54, 224, 168, 10, 0x0841);
    fb.setTextSize(1);
    fb.setTextColor(themes.get().text, 0x0841);
    fb.setCursor(252, 72); fb.print("Last favorite:");
    fb.setTextColor(themes.get().dim, 0x0841);
    fb.setCursor(252, 96);
    char favPath[72];
    strncpy(favPath, rt.favoriteLastPath[0] ? rt.favoriteLastPath : "favadd current track", sizeof(favPath) - 1);
    favPath[sizeof(favPath) - 1] = 0;
    fb.print(favPath);
    fb.setCursor(252, 144); fb.printf("Load/save: %lu ms", (unsigned long)rt.favoriteLastMs);
    fb.setCursor(252, 170); fb.print("No SD scan required");

    fb.drawRoundRect(18, 238, 444, 50, 8, themes.get().border);
    fb.setTextSize(1);
    fb.setTextColor(themes.get().text, themes.get().bg);
    fb.setCursor(30, 252); fb.print("Commands: fav, favadd, favload, favsave, favclear");
    fb.setCursor(30, 270); fb.print("Favorites list is printed to Serial Monitor.");

    footer("Favorites UI v9.1.1");
    present();
  }


  void libraryCategoryScreen(const char* title, const char* commandHint) {
    screen = SCR_LIBRARY;
    fb.fillScreen(themes.get().bg);
    fb.fillRoundRect(10, 8, 460, 34, 8, themes.get().panel);
    text(22, 18, title, themes.get().text, themes.get().panel, 2);
    fb.setTextSize(1);
    fb.setTextColor(themes.get().dim, themes.get().panel);
    fb.setCursor(350, 20);
    fb.print("v9.1.1");

    fb.drawRoundRect(18, 54, 210, 176, 10, themes.get().border);
    fb.setTextSize(2);
    fb.setTextColor(themes.get().accent, themes.get().bg);
    fb.setCursor(34, 72); fb.print("Index");
    fb.setTextColor(themes.get().text, themes.get().bg);
    fb.setCursor(34, 108); fb.printf("Artists %d", rt.libraryArtistCount);
    fb.setCursor(34, 136); fb.printf("Albums  %d", rt.libraryAlbumCount);
    fb.setCursor(34, 164); fb.printf("Genres  %d", rt.libraryGenreCount);
    fb.setCursor(34, 192); fb.printf("Folders %d", rt.libraryFolderCount);

    fb.fillRoundRect(238, 54, 224, 176, 10, 0x0841);
    fb.setTextSize(1);
    fb.setTextColor(themes.get().text, 0x0841);
    fb.setCursor(252, 72); fb.print("Last view:");
    fb.setTextColor(themes.get().dim, 0x0841);
    fb.setCursor(252, 96); fb.print(rt.libraryLastView);
    fb.setCursor(252, 126); fb.printf("Tracks : %d", rt.dbTrackCount > 0 ? rt.dbTrackCount : rt.playlistCount);
    fb.setCursor(252, 148); fb.printf("Build  : %lu ms", (unsigned long)rt.libraryLastMs);
    fb.setCursor(252, 170); fb.print("Files  : artist/album/");
    fb.setCursor(252, 188); fb.print("genre/folder.idx");

    fb.drawRoundRect(18, 244, 444, 48, 8, themes.get().border);
    fb.setTextSize(1);
    fb.setTextColor(themes.get().text, themes.get().bg);
    fb.setCursor(30, 258); fb.print(commandHint);
    fb.setCursor(30, 276); fb.print("Serial Monitor prints first 32 rows.");

    footer("Library Index UI v9.1.1");
    present();
  }

  void multimediaScreen() {
    screen = SCR_STATUS;
    header("MEDIA");
    text(25, 58, "Media Foundation", themes.get().accent, themes.get().bg, 3);

    uint32_t rate = rt.mediaSampleRate ? rt.mediaSampleRate : (rt.wavSampleRate ? rt.wavSampleRate : 44100);
    uint16_t ch = rt.mediaChannels ? rt.mediaChannels : (rt.wavChannels ? rt.wavChannels : 2);
    char meta[32];
    snprintf(meta, sizeof(meta), "%.1f kHz • %s", rate / 1000.0f, ch == 2 ? "Stereo" : "Mono");

    fb.setTextSize(2);
    fb.setTextColor(themes.get().text, themes.get().bg);
    fb.setCursor(25, 100); fb.printf("%s", rt.mediaTitle);

    fb.setTextSize(1);
    fb.setTextColor(0x7BEF, themes.get().bg);
    fb.setCursor(25, 128); fb.printf("%s  |  %s", MediaEngine::codecName(rt.mediaCodec), meta);
    fb.setCursor(25, 148); fb.printf("State: %s", MediaEngine::stateName(rt.mediaState));

    fb.setTextSize(2);
    fb.setTextColor(themes.get().ok, themes.get().bg);
    fb.setCursor(25, 182); fb.printf("BUF %u%%", (unsigned int)rt.mediaBufferHealth);
    fb.setTextColor(themes.get().text, themes.get().bg);
    fb.setCursor(170, 182); fb.printf("UDR %lu", (unsigned long)rt.mediaUnderruns);
    fb.setCursor(310, 182); fb.printf("SW %lu", (unsigned long)rt.mediaShortWrites);

    fb.setTextSize(1);
    fb.setTextColor(themes.get().border, themes.get().bg);
    fb.setCursor(25, 222); fb.printf("Files:%d  PL:%d  Q:%d", rt.fileIndexCount, rt.playlistCount, rt.queueCount);
    fb.setCursor(25, 242); fb.print("Alias: update scan db list q resume p s info");

    widgets.progress(fb, 25, 268, 300, 18, rt.mediaProgress, themes.get().ok, themes.get().bg);

    footer("v9.1.1 Player UI");
    present();
  }

  void webRadioScreen() {
    screen = SCR_STATUS;
    header("WEB RADIO");
    text(32, 70, "Internet Radio", themes.get().accent, themes.get().bg, 3);
    fb.setTextSize(2);
    fb.setTextColor(themes.get().text, themes.get().bg);
    fb.setCursor(32, 130); fb.printf("Status: %.20s", rt.radioStatus);
    fb.setCursor(32, 165); fb.printf("Station: %.30s", rt.radioStation[0] ? rt.radioStation : "-");
    fb.setTextSize(1);
    fb.setTextColor(themes.get().border, themes.get().bg);
    fb.setCursor(32, 205); fb.printf("Title: %.56s", rt.radioTitle[0] ? rt.radioTitle : "-");
    fb.setCursor(32, 225); fb.printf("Connect:%lu  Reconnect:%lu  Drop:%lu",
      (unsigned long)rt.radioConnectAttempts,
      (unsigned long)rt.radioReconnects,
      (unsigned long)rt.radioCommandsDropped);
    footer("OK - HOME");
    present();
  }

  void radioScreen() {
    screen = SCR_STATUS;
    header("RADIO");
    text(32, 70, "FM RADIO", themes.get().warn, themes.get().bg, 3);
    fb.setTextSize(2);
    fb.setTextColor(themes.get().text, themes.get().bg);
    fb.setCursor(32, 140); fb.print("SI4735 module");
    fb.setCursor(32, 175); fb.print("Status: not connected");
    footer("OK - HOME");
    present();
  }

  void bluetoothScreen() {
    screen = SCR_STATUS;
    header("BLUETOOTH");
    text(32, 70, "BLUETOOTH AUDIO", themes.get().accent, themes.get().bg, 3);
    fb.setTextSize(2);
    fb.setTextColor(themes.get().text, themes.get().bg);
    fb.setCursor(32, 140); fb.print("A2DP module");
    fb.setCursor(32, 175); fb.print("Status: foundation ready");
    footer("OK - HOME");
    present();
  }

  void openSelected() {
    msg = String("OPEN: ") + homeItems[sel];
    if (sel == HOME_PLAYER) playerScreen();
    else if (sel == HOME_WEB_RADIO) webRadioScreen();
    else if (sel == HOME_RADIO) radioScreen();
    else if (sel == HOME_BLUETOOTH) bluetoothScreen();
    else settings();
  }

  void tickSerialCommand(const String& raw) {
    String original = raw;
    original.trim();
    String c = original;
    c.toLowerCase();

    // v9.8-alpha1 WiFi Foundation. Preserve original case for SSID/password.
    String wifiLower = c;
    if (wifiLower.startsWith("wifi connect ") || wifiLower.startsWith("wifi apsta ") || wifiLower.startsWith("wifi save ")) {
      const bool apsta = wifiLower.startsWith("wifi apsta ");
      const bool saveOnly = wifiLower.startsWith("wifi save ");
      const int prefixLen = apsta ? 11 : (saveOnly ? 10 : 13);
      String args = original.substring(prefixLen);
      args.trim();
      int split = args.indexOf(' ');
      String ssid = split < 0 ? args : args.substring(0, split);
      String pass = split < 0 ? String("") : args.substring(split + 1);
      ssid.trim(); pass.trim();
      if (!ssid.length()) {
        if (apsta) Serial.println("[NET] usage: wifi apsta <ssid> <password>");
        else if (saveOnly) Serial.println("[NET] usage: wifi save <ssid> <password>");
        else Serial.println("[NET] usage: wifi connect <ssid> <password>");
        return;
      }
      strncpy(rt.wifiPendingSsid, ssid.c_str(), sizeof(rt.wifiPendingSsid) - 1);
      rt.wifiPendingSsid[sizeof(rt.wifiPendingSsid) - 1] = 0;
      strncpy(rt.wifiPendingPassword, pass.c_str(), sizeof(rt.wifiPendingPassword) - 1);
      rt.wifiPendingPassword[sizeof(rt.wifiPendingPassword) - 1] = 0;
      if (saveOnly) rt.wifiSaveRequest = true;
      else if (apsta) rt.wifiApStaConnectRequest = true;
      else rt.wifiStaConnectRequest = true;
      Serial.printf("[NET] %s requested ssid='%s' password=%s\n", saveOnly ? "SAVE" : (apsta ? "AP+STA" : "STA"), rt.wifiPendingSsid, pass.length() ? "set" : "open");
      return;
    }

    // v9.4-alpha2 Theme Engine: instant switching + NVS persistence.
    if (c == "theme" || c == "theme status" || c == "theme list") {
      themes.printList(Serial);
      return;
    }
    if (c.startsWith("theme ")) {
      String requested = c.substring(6);
      requested.trim();
      if (!themes.set(requested, true)) {
        Serial.printf("[THEME] unknown='%s'\n", requested.c_str());
        themes.printList(Serial);
        return;
      }
      Serial.printf("[THEME] active=%s saved=1\n", themes.name());
      resetPlayerPartialCache();
      switch (screen) {
        case SCR_PLAYER: playerScreen(); break;
        case SCR_LIBRARY: libraryScreen(); break;
        case SCR_ALBUMS: albumBrowserScreen(); break;
        case SCR_FAVORITES: favoritesScreen(); break;
        case SCR_HOME: home(); break;
        default: home(); break;
      }
      return;
    }

    // v8.0.2: EventBus + ResumeEngine on top of DB Engine Pro.
    if (c == "?") c = "h";
    else if (c == "st") c = "status";
    else if (c == "hl") c = "health";
    else if (c == "rb") c = "reboot";
    else if (c == "now") c = "player";
    else if (c == "mi" || c == "meta" || c == "info") c = "minfo";
    else if (c == "m" || c == "mm") c = "media";
    else if (c == "q") c = "queue";
    else if (c == "qadd") c = "queueadd";
    else if (c == "qclear") c = "queueclear";
    else if (c == "qnext") c = "queuenext";
    else if (c == "qprev") c = "queueprev";
    else if (c == "qsave") c = "queuesave";
    else if (c == "qload") c = "queueload";
    else if (c == "qshuffle" || c == "shuffle" || c == "smartshuffle") c = "queueshuffle";
    else if (c == "qmode" || c == "mode" || c == "repeat") c = "queuemode";
    else if (c == "pipeline" || c == "pipe") c = "pipeline";
    else if (c == "diag" || c == "diagnostics") c = "diag";
    else if (c == "library" || c == "lib" || c == "music" || c == "musiclib") c = "library";
    else if (c == "artists" || c == "artist") c = "artists";
    else if (c == "albums" || c == "album" || c == "alb" || c == "albumui") c = "albums";
    else if (c == "genres" || c == "genre") c = "genres";
    else if (c == "folders" || c == "folder") c = "folders";
    else if (c == "recent") c = "recent";
    else if (c == "most" || c == "mostplayed") c = "most";
    else if (c == "stats" || c == "libstats") c = "libstats";
    else if (c == "libbuild" || c == "librarybuild") c = "libbuild";
    else if (c == "favorites" || c == "favorite" || c == "fav" || c == "favs") c = "favorites";
    else if (c == "favadd" || c == "favoriteadd") c = "favoriteadd";
    else if (c == "favclear" || c == "favoriteclear") c = "favoriteclear";
    else if (c == "favsave" || c == "favoritesave") c = "favoritesave";
    else if (c == "favload" || c == "favoriteload") c = "favoriteload";
    else if (c == "searchui" || c == "searchscreen" || c == "search") c = "searchui";
    else if (c == "resume" || c == "rinfo") c = "resume";
    else if (c == "rsave") c = "resumesave";
    else if (c == "rload") c = "resumeload";
    else if (c == "rclear") c = "resumeclear";
    else if (c == "mstop" || c == "s") c = "stop";
    else if (c == "mplay" || c == "p") c = "play";
    else if (c == "mhealth") c = "health";
    else if (c == "ls") c = "index";
    else if (c == "playlist" || c == "plist" || c == "list") c = "pl";
    else if (c == "update" || c == "upd" || c == "dbupdate") c = "dbupdate";
    else if (c == "scan" || c == "rescan" || c == "dbscan" || c == "artscan") c = "dbscan";
    else if (c == "dbload") c = "dbload";
    else if (c == "dbfast") c = "dbfast";
    else if (c == "dbtest" || c == "testdb" || c == "dbcheck") c = "dbtest";
    else if (c == "dbclear") c = "dbclear";
    else if (c == "db" || c == "artdb") c = "db";
    else if (c == "next" || c == "mnext" || c == "plnext") c = "plnext";
    else if (c == "prev" || c == "mprev" || c == "plprev") c = "plprev";
    else if (c == "web" || c == "webinfo") c = "webinfo";
    else if (c == "web on" || c == "webon") c = "webon";
    else if (c == "web off" || c == "weboff") c = "weboff";
    else if (c == "net ap" || c == "wifi ap") c = "netap";
    else if (c == "net off" || c == "wifi off") c = "netoff";
    else if (c == "wifi scan") c = "wifiscan";
    else if (c == "wifi auto on") c = "wifiautoon";
    else if (c == "wifi auto off") c = "wifiautooff";
    else if (c == "wifi auto") c = "wifistatus";
    else if (c == "wifi fallback on") c = "wififallbackon";
    else if (c == "wifi fallback off") c = "wififallbackoff";
    else if (c == "wifi forget") c = "wififorget";
    else if (c == "wifi boot") c = "wifiboot";
    else if (c == "wifi stats") c = "wifistatus";
    else if (c == "wifi status" || c == "wifi ip") c = "wifistatus";
    else if (c == "wifi disconnect") c = "wifidisconnect";
    else if (c == "wifi load" || c == "wifi sta") c = "wifiload";
    else if (c == "wifi save") c = "wifisaveusage";
    else if (c == "ota info") c = "otainfo";
    else if (c == "ota sd") c = "otasd";
    else if (c == "radio" || c == "radio info") c = "radioinfo";
    else if (c == "radio stop") c = "radiostop";
    else if (c == "stability" || c == "stab") c = "stability";
    else if (c == "polish" || c == "commercial") c = "polish";
    else if (c == "burnin start" || c == "burnin") c = "burninstart";
    else if (c == "burnin stop") c = "burninstop";
    else if (c == "autoplay" || c == "autoplay status" || c == "aplay") c = "autoplay";
    else if (c == "autoplay on" || c == "aplay on") c = "autoplayon";
    else if (c == "autoplay off" || c == "aplay off") c = "autoplayoff";

    // v8.0.4 Smart Queue command parsing.
    bool isRepeatCommand = c.startsWith("repeat ") || c.startsWith("qrepeat ");
    if (isRepeatCommand) {
      int sp = c.indexOf(' ');
      String arg = sp >= 0 ? c.substring(sp + 1) : String("");
      arg.trim();
      if (arg == "off" || arg == "0") c = "queuerepeat0";
      else if (arg == "one" || arg == "1" || arg == "track") c = "queuerepeat1";
      else if (arg == "all" || arg == "2" || arg == "playlist") c = "queuerepeat2";
      else c = "queuemode";
    }

    // v7.7.1 DB Engine Pro: find <text> and findall <text> search SD-backed media database.
    bool isFindAllCommand = c.startsWith("findall ") || c.startsWith("searchall ");
    bool isFindCommand = isFindAllCommand || c.startsWith("find ") || c.startsWith("search ");

    if (rt.scanLock || rt.dbScanBusy || rt.dbScanRequest) {
      bool allowed = (c == "h" || c == "status" || c == "health" || c == "db" || c == "media" || c == "minfo" || c == "pl" || c == "queue" || c == "resume" || c == "library" || c == "artists" || c == "albums" || c == "genres" || c == "folders" || c == "recent" || c == "most" || c == "favorites" || c == "searchui" || c == "diag" || c == "services" || c == "home" || c == "stability" || c == "polish" || c == "burninstop" || c == "render" || c == "autoplay");
      if (!allowed) {
        Serial.print("[SCAN] command queued/ignored while scanning: ");
        Serial.println(c);
        Serial.println("[SCAN] Please wait for SD scan complete");
        scanLockScreen();
        return;
      }
    }

    if (isFindCommand) {
      int sp = c.indexOf(' ');
      String q = sp >= 0 ? c.substring(sp + 1) : String("");
      q.trim();
      if (!q.length()) {
        Serial.println("[FIND] usage: find <text> or findall <text>");
      } else {
        strncpy(rt.dbFindQuery, q.c_str(), sizeof(rt.dbFindQuery) - 1);
        rt.dbFindQuery[sizeof(rt.dbFindQuery) - 1] = 0;
        rt.dbFindLimit = isFindAllCommand ? 64 : 16;
        rt.dbFindRequest = true;
        Serial.printf("[FIND] request: %s limit=%d\n", rt.dbFindQuery, rt.dbFindLimit);
        multimediaScreen();
      }
    }
    else if (c == "art" || c == "art info" || c == "art cache") {
      ArtworkCache::instance().printInfo();
    }
    else if (c == "art clear") {
      ArtworkCache::instance().clear();
      Serial.println("[ART] clear requested");
      resetPlayerPartialCache();
      if (screen == SCR_PLAYER) playerScreen();
    }
    else if (c == "art reload") {
      rt.artworkReloadRequest = true;
      Serial.println("[ART] reload queued on Core0");
    }
    else if (c == "vu" || c == "vu status") {
      printVuStatus();
    }
    else if (c == "player theme" || c == "player layout") {
      Serial.println("[PLAYER-UI] layout=artwork (fixed 480x320 approved geometry)");
    }
    else if (c.startsWith("player theme ") || c.startsWith("player layout ")) {
      playerLayoutMode = PLAYER_LAYOUT_ARTWORK;
      savePlayerLayout();
      resetPlayerPartialCache();
      Serial.println("[PLAYER-UI] layout=artwork saved=1");
      if (screen == SCR_PLAYER) playerScreen();
    }
    else if (c.startsWith("vu style ")) {
      String arg = c.substring(9); arg.trim();
      bool valid = true;
      if (arg == "rect") playerVuStyle = VU_RECT;
      else if (arg == "dot") playerVuStyle = VU_DOT;
      else if (arg == "thin") playerVuStyle = VU_THIN;
      else if (arg == "line") playerVuStyle = VU_LINE;
      else valid = false;
      if (!valid) Serial.println("[VU] invalid style; use rect, dot, thin or line");
      else {
        saveVuSettings();
        Serial.printf("[VU] style=%s saved=1\n", vuStyleName());
        if (screen == SCR_PLAYER) updatePlayerVuOnly("style");
      }
    }
    else if (c.startsWith("vu fps ")) {
      int v = c.substring(7).toInt();
      if (v < 10 || v > 30) Serial.println("[VU] invalid fps; use 10..30");
      else { playerVuFps = (uint8_t)v; saveVuSettings(); Serial.printf("[VU] fps=%u saved=1\n", playerVuFps); if (screen == SCR_PLAYER) updatePlayerVuOnly("fps"); }
    }
    else if (c.startsWith("vu peak ")) {
      String arg = c.substring(8); arg.trim();
      if (arg != "on" && arg != "off") Serial.println("[VU] invalid peak; use on/off");
      else {
        playerVuPeakEnabled = (arg == "on");
        if (!playerVuPeakEnabled) playerPeakL = playerPeakR = 0;
        saveVuSettings();
        Serial.printf("[VU] peak=%s saved=1\n", playerVuPeakEnabled ? "on" : "off");
        if (screen == SCR_PLAYER) updatePlayerVuOnly("peak");
      }
    }
    else if (c.startsWith("vu hold ")) {
      int v = c.substring(8).toInt();
      if (v < 50 || v > 1500) Serial.println("[VU] invalid hold; use 50..1500 ms");
      else { playerVuPeakHoldMs = (uint16_t)v; saveVuSettings(); Serial.printf("[VU] hold=%ums saved=1\n", playerVuPeakHoldMs); if (screen == SCR_PLAYER) updatePlayerVuOnly("hold"); }
    }
    else if (c.startsWith("vu decay ")) {
      int v = c.substring(9).toInt();
      if (v < 1 || v > 10) Serial.println("[VU] invalid decay; use 1..10");
      else { playerVuDecay = (uint8_t)v; saveVuSettings(); Serial.printf("[VU] decay=%u saved=1\n", playerVuDecay); if (screen == SCR_PLAYER) updatePlayerVuOnly("decay"); }
    }
    else if (c == "vu save") {
      saveVuSettings(); Serial.println("[VU] settings saved");
    }
    else if (c == "vuseg" || c == "vusegments") {
      Serial.printf("[VU] segments=%u range=%u..%u\n",
                    playerVuSegments,
                    PLAYER_VU_SEGMENTS_MIN,
                    PLAYER_VU_SEGMENTS_MAX);
    }
    else if (c.startsWith("vuseg ") || c.startsWith("vusegments ")) {
      int sp = c.indexOf(' ');
      String arg = sp >= 0 ? c.substring(sp + 1) : String("");
      arg.trim();

      int requested = playerVuSegments;
      bool valid = true;

      if (arg == "+") {
        requested = (playerVuSegments >= PLAYER_VU_SEGMENTS_MAX)
                  ? PLAYER_VU_SEGMENTS_MIN
                  : (playerVuSegments + 1);
      } else if (arg == "-") {
        requested = (playerVuSegments <= PLAYER_VU_SEGMENTS_MIN)
                  ? PLAYER_VU_SEGMENTS_MAX
                  : (playerVuSegments - 1);
      } else {
        // Strict decimal parser: reject garbage instead of String::toInt() silently returning 0.
        if (!arg.length()) {
          valid = false;
        } else {
          for (size_t i = 0; i < arg.length(); ++i) {
            if (!isDigit((unsigned char)arg[i])) {
              valid = false;
              break;
            }
          }
        }
        if (valid) requested = arg.toInt();
      }

      if (!valid || requested < PLAYER_VU_SEGMENTS_MIN || requested > PLAYER_VU_SEGMENTS_MAX) {
        Serial.printf("[VU] invalid segment argument='%s'; use %u..%u, + or -\n",
                      arg.c_str(),
                      PLAYER_VU_SEGMENTS_MIN,
                      PLAYER_VU_SEGMENTS_MAX);
      } else {
        playerVuSegments = (uint8_t)requested;
        saveVuSettings();
        Serial.printf("[VU] segment count set to %u saved=1\n", playerVuSegments);
        if (screen == SCR_PLAYER) updatePlayerVuOnly("segments");
      }
    }

    else if (c == "h") {
      Serial.println("============================================================");
      Serial.println(" IQ200 OS v9.5-alpha3.1 - COMPLETE COMMAND HELP");
      Serial.println("============================================================");
      Serial.println("HELP/UI : h/? | home | back | player/now | settings");
      Serial.println("SCREENS : status/st | health/hl | diag | media/m | minfo/mi/info");
      Serial.println("SYSTEM  : psram | display | enc | audio | wifi | sd | files | tasks");
      Serial.println("SYSTEM  : scheduler | services | apps | windows | coreos | render | tone");
      Serial.println("SYSTEM  : reboot/rb | polish/commercial | stability/stab");
      Serial.println("PLAYBACK: play/mplay/p | playwav | wav | stop/mstop/s | next/plnext | prev/plprev");
      Serial.println("PLAYBACK: autoplay on | autoplay off | autoplay status");
      Serial.println("PLAYLIST: pl/list | plscan | pladd | plclear | index/ls");
      Serial.println("QUEUE   : queue/q | qadd | qclear | qnext | qprev | qsave | qload");
      Serial.println("QUEUE   : qshuffle/shuffle | repeat off | repeat one | repeat all");
      Serial.println("PIPELINE: pipeline/pipe");
      Serial.println("LIBRARY : library/lib/music | artists | albums | genres | folders");
      Serial.println("LIBRARY : recent | most | libstats | libbuild | searchui/search");
      Serial.println("SEARCH  : find <text> | findall <text>");
      Serial.println("FAVORITE: favorites/fav | favadd | favload | favsave | favclear");
      Serial.println("RESUME  : resume/rinfo | rsave | rload | rclear");
      Serial.println("DATABASE: db | dbload | dbfast | dbtest | dbclear");
      Serial.println("DATABASE: scan/rescan/dbscan | update/upd/dbupdate");
      Serial.println("VU      : vu status | vu style rect|dot|thin|line | vu fps 10..30");
      Serial.println("ART     : art | art info | art cache | art clear | art reload");
      Serial.println("VU      : vu peak on|off | vu hold 50..1500 | vu decay 1..10");
      Serial.println("VU      : vuseg 4..24 | vu save");
      Serial.println("THEME   : theme list | theme bluepro|emerald|amber|purple|ice|oled|carbon|matrix");
      Serial.println("NAV2    : nav status | nav preview on | nav preview off");
      Serial.println("NAV2    : nav delay 200..1500");
      Serial.println("NETWORK : wifi scan | wifi status | wifi connect <ssid> <password>");
      Serial.println("NETWORK : wifi save <ssid> <password>");
      Serial.println("NETWORK : wifi load | wifi disconnect | wifi ap | wifi apsta <ssid> <password>");
      Serial.println("NETWORK : web | web on | web off | net off | http://iq200.local/");
      Serial.println("OTA     : ota info | ota sd | ota");
      Serial.println("RADIO   : radio | radio stop");
      Serial.println("BURN-IN : burnin start | burnin stop");
      Serial.println("LOG     : log on | log off | log status | log reset");
      Serial.println("LOG     : log rate 250..10000");
      Serial.println("BLACKBOX: bb on | bb off | bb status | bb clear | bb dump [count]");
      Serial.println("PROFILER: perf | ui | ui status | ui reset");
      Serial.println("ALIASES : st hl rb now mi info m q p s ls list scan rescan upd fav");
      Serial.println("============================================================");
    }

    else if (c == "autoplay") {
      rt.autoplayInfoRequest = true;
      Serial.println("[AUTOPLAY] status requested");
      multimediaScreen();
    }
    else if (c == "autoplayon") {
      rt.autoplayOnRequest = true;
      Serial.println("[AUTOPLAY] enable requested");
      multimediaScreen();
    }
    else if (c == "autoplayoff") {
      rt.autoplayOffRequest = true;
      Serial.println("[AUTOPLAY] disable requested");
      multimediaScreen();
    }

    else if (c == "wifiscan") {
      rt.wifiScanNowRequest = true;
      Serial.println("[NET] WiFi scan requested");
    }
    else if (c == "wifistatus") {
      rt.wifiStatusRequest = true;
      Serial.println("[NET] WiFi status requested");
    }
    else if (c == "wifidisconnect") {
      rt.wifiDisconnectRequest = true;
      Serial.println("[NET] WiFi STA disconnect requested");
    }
    else if (c == "wifiload") {
      rt.wifiLoadRequest = true;
      Serial.println("[NET] connect using saved profile requested");
    }
    else if (c == "wifisaveusage") {
      Serial.println("[NET] usage: wifi save <ssid> <password>");
    }
    else if (c == "wifiautoon") { rt.wifiAutoOnRequest = true; Serial.println("[NET] AutoConnect ON requested"); }
    else if (c == "wifiautooff") { rt.wifiAutoOffRequest = true; Serial.println("[NET] AutoConnect OFF requested"); }
    else if (c == "wififallbackon") { rt.wifiFallbackOnRequest = true; Serial.println("[NET] Fallback AP ON requested"); }
    else if (c == "wififallbackoff") { rt.wifiFallbackOffRequest = true; Serial.println("[NET] Fallback AP OFF requested"); }
    else if (c == "wififorget") { rt.wifiForgetRequest = true; Serial.println("[NET] forget saved profile requested"); }
    else if (c == "wifiboot") { rt.wifiBootRequest = true; Serial.println("[NET] boot policy restart requested"); }
    else if (c == "webinfo") {
      rt.webInfoRequest = true;
      Serial.printf("[WEB] enabled=%d running=%d ip=%s port=%u requests=%lu status=%s\n", rt.webEnabled ? 1 : 0, rt.webRunning ? 1 : 0, rt.webIp, rt.webPort, (unsigned long)rt.webRequests, rt.webStatus);
      status();
    }
    else if (c == "webon") {
      rt.webEnableRequest = true;
      Serial.println("[WEB] enable requested. If IP is 0.0.0.0, run: net ap");
    }
    else if (c == "weboff") {
      rt.webDisableRequest = true;
      Serial.println("[WEB] disable requested");
    }
    else if (c == "netap") {
      rt.netApRequest = true;
      rt.webEnableRequest = true;
      Serial.println("[NET] AP + Web requested: SSID IQ200-OS, URL http://192.168.4.1/");
    }
    else if (c == "netoff") {
      rt.netOffRequest = true;
      Serial.println("[NET] network off requested");
    }
    else if (c == "otainfo") {
      rt.otaInfoRequest = true;
      Serial.println("[OTA] info requested");
    }
    else if (c == "otasd") {
      rt.otaSdRequest = true;
      Serial.println("[OTA] SD OTA validation requested");
    }
    else if (c == "radioinfo") {
      rt.radioInfoRequest = true;
      Serial.println("[RADIO] info requested");
    }
    else if (c == "radiostop") {
      rt.radioStopRequest = true;
      Serial.println("[RADIO] stop requested");
    }
    else if (c == "stability") {
      rt.stabilityInfoRequest = true;
      Serial.println("[STAB] info requested");
    }
    else if (c == "polish") {
      rt.commercialInfoRequest = true;
      Serial.println("[POLISH] info requested");
    }
    else if (c == "burninstart") {
      rt.burninStartRequest = true;
      Serial.println("[BURNIN] start requested");
    }
    else if (c == "burninstop") {
      rt.burninStopRequest = true;
      Serial.println("[BURNIN] stop requested");
    }
    else if (c == "home") home();
    else if (c == "status") status();
    else if (c == "psram") psram();
    else if (c == "display") displayTest();
    else if (c == "enc") encoders();
    else if (c == "audio") audioScreen();
    else if (c == "wifi") wifiScreen();
    else if (c == "sd") sdScreen();
    else if (c == "files") filesScreen();
    else if (c == "library") { rt.libraryStatsRequest = true; libraryScreen(); }
    else if (c == "artists") { rt.artistListRequest = true; libraryCategoryScreen("ARTISTS", "Command: artists  | build: libbuild"); }
    else if (c == "albums") { rt.albumListRequest = true; albumBrowserScreen(); }
    else if (c == "genres") { rt.genreListRequest = true; libraryCategoryScreen("GENRES", "Command: genres  | source: genre.idx"); }
    else if (c == "folders") { rt.folderListRequest = true; libraryCategoryScreen("FOLDERS", "Command: folders | source: folder.idx"); }
    else if (c == "recent") { rt.recentListRequest = true; libraryCategoryScreen("RECENT", "Command: recent | file: recent.db"); }
    else if (c == "most") { rt.mostListRequest = true; libraryCategoryScreen("MOST PLAYED", "Command: most | file: mostplayed.db"); }
    else if (c == "libstats") { rt.libraryStatsRequest = true; libraryCategoryScreen("LIBRARY STATS", "Command: libstats | file: library.meta"); }
    else if (c == "libbuild" || c == "libbuild force") { rt.libraryBuildForce = (c == "libbuild force"); rt.libraryBuildRequest = true; libraryCategoryScreen("LIBRARY BUILD", rt.libraryBuildForce ? "Force rebuilding indexes..." : "Checking/rebuilding indexes..."); }
    else if (c == "favorites") { rt.favoriteListRequest = true; favoritesScreen(); }
    else if (c == "searchui") searchScreen();
    else if (c == "player") playerScreen();
    else if (c == "settings") settings();
    else if (c == "tasks") taskMonitor();
    else if (c == "diag") diagScreen();
    else if (c == "health") healthScreen();
    else if (c == "scheduler") schedulerScreen();
    else if (c == "apps") appManagerScreen();
    else if (c == "windows") windowManagerScreen();
    else if (c == "coreos") coreOsScreen();
    else if (c == "ota") otaScreen();
    else if (c == "services") servicesScreen();
    else if (c == "render") renderScreen();
    else if (c == "media" || c == "minfo" || c == "pl") {
      Serial.printf("[MEDIA] %s  %s  %u%%\n", rt.mediaTitle, MediaEngine::stateName(rt.mediaState), (unsigned int)rt.mediaProgress);
      Serial.printf("[PL] count=%d index=%d current=%s\n", rt.playlistCount, rt.playlistIndex + 1, rt.playlistCurrent);
      multimediaScreen();
    }
    else if (c == "dbupdate") {
      if (rt.dbUpdateBusy || rt.dbUpdateRequest) {
        Serial.println("[DBUPD] already pending/running");
      } else {
        rt.dbUpdateRequest = true;
        Serial.println("[DBUPD] request accepted. Fast DB check first, full scan only if required.");
      }
      multimediaScreen();
    }
    else if (c == "plscan" || c == "dbscan") { rt.dbScanRequest = true; rt.scanProgress = 0; strncpy(rt.scanMessage, "SD scan requested", sizeof(rt.scanMessage)-1); Serial.println("[SCAN] request accepted. All media commands will wait until finish."); scanLockScreen(); }
    else if (c == "dbload") { rt.dbLoadRequest = true; multimediaScreen(); }
    else if (c == "dbfast") { rt.dbInfoRequest = true; Serial.println("[DBFAST] use db command to inspect manifest; boot automatically skips full scan when valid"); multimediaScreen(); }
    else if (c == "dbclear") { rt.dbClearRequest = true; multimediaScreen(); }
    else if (c == "db") { rt.dbInfoRequest = true; multimediaScreen(); }
    else if (c == "dbtest") { rt.dbTestRequest = true; multimediaScreen(); }
    else if (c == "pladd") { rt.playlistAddTestRequest = true; multimediaScreen(); }
    else if (c == "plclear") { rt.playlistClearRequest = true; multimediaScreen(); }
    else if (c == "plnext") {
      // Manual navigation owns the transition: cancel any queued EOF AutoNext.
      rt.playlistNextRequest = false;
      rt.playlistNextAutoPlayRequest = false;
      rt.playlistPrevRequest = false;
      rt.navManualControlPending = true;
      if (rt.navPreviewEnabled) { rt.navPendingDelta++; rt.navLastInputMs = millis(); rt.navCommitPending = true; }
      else rt.playlistNextRequest = true;
      // alpha47: when already in Player, keep the existing framebuffer. The
      // navigation transition will update ART/title/meta/progress/state through
      // partial widgets. Calling playerScreen() here caused one full redraw.
      if (screen != SCR_PLAYER) playerScreen();
    }
    else if (c == "plprev") {
      // Manual navigation owns the transition: cancel any queued EOF AutoNext.
      rt.playlistNextRequest = false;
      rt.playlistNextAutoPlayRequest = false;
      rt.playlistPrevRequest = false;
      rt.navManualControlPending = true;
      if (rt.navPreviewEnabled) { rt.navPendingDelta--; rt.navLastInputMs = millis(); rt.navCommitPending = true; }
      else rt.playlistPrevRequest = true;
      if (screen != SCR_PLAYER) playerScreen();
    }
    else if (c == "nav status") {
      Serial.printf("[NAV2] preview=%d delay=%ums active=%d pending=%ld moves=%lu commits=%lu\n",
        rt.navPreviewEnabled ? 1 : 0, (unsigned)rt.navCommitDelayMs, rt.navPreviewActive ? 1 : 0,
        (long)rt.navPendingDelta, (unsigned long)rt.navPreviewMoves, (unsigned long)rt.navCommits);
    }
    else if (c == "nav preview on") { rt.navPreviewEnabled = true; Serial.println("[NAV2] preview=ON"); }
    else if (c == "nav preview off") { rt.navPreviewEnabled = false; rt.navPendingDelta = 0; rt.navCommitPending = false; Serial.println("[NAV2] preview=OFF"); }
    else if (c.startsWith("nav delay ")) {
      int v = c.substring(10).toInt();
      if (v < 200 || v > 1500) Serial.println("[NAV2] delay range 200..1500 ms");
      else { rt.navCommitDelayMs = (uint16_t)v; Serial.printf("[NAV2] delay=%dms\n", v); }
    }
    else if (c == "pipeline") {
      Serial.printf("[PIPE] file=%lu/%lu meta=%lu/%lu db=%lu/%lu dropped=%lu events=%lu\n",
        (unsigned long)rt.pipelineFileQueueDepth, (unsigned long)rt.pipelineFileQueueSize,
        (unsigned long)rt.pipelineMetaQueueDepth, (unsigned long)rt.pipelineMetaQueueSize,
        (unsigned long)rt.pipelineDbQueueDepth, (unsigned long)rt.pipelineDbQueueSize,
        (unsigned long)rt.pipelineDropped, (unsigned long)rt.eventBusPosts);
      rt.pipelineInfoRequest = true;
      multimediaScreen();
    }
    else if (c == "queue") {
      Serial.printf("[QUEUE] count=%d index=%d shuffle=%s repeat=%d current=%s\n", rt.queueCount, rt.queueIndex + 1, rt.queueShuffleSmart ? "ON" : "OFF", rt.queueRepeatMode, rt.queueCurrent);
      rt.queueListRequest = true;
      multimediaScreen();
    }
    else if (c == "queueadd") { rt.queueAddCurrentRequest = true; multimediaScreen(); }
    else if (c == "queueclear") { rt.queueClearRequest = true; multimediaScreen(); }
    else if (c == "queuenext") { rt.queueNextRequest = true; multimediaScreen(); }
    else if (c == "queueprev") { rt.queuePrevRequest = true; multimediaScreen(); }
    else if (c == "queuesave") { rt.queueSaveRequest = true; multimediaScreen(); }
    else if (c == "queueload") { rt.queueLoadRequest = true; multimediaScreen(); }
    else if (c == "queueshuffle") { rt.queueShuffleToggleRequest = true; multimediaScreen(); }
    else if (c == "queuemode") { rt.queueModeInfoRequest = true; multimediaScreen(); }
    else if (c == "queuerepeat0") { rt.queueRepeatSetRequest = 0; multimediaScreen(); }
    else if (c == "queuerepeat1") { rt.queueRepeatSetRequest = 1; multimediaScreen(); }
    else if (c == "queuerepeat2") { rt.queueRepeatSetRequest = 2; multimediaScreen(); }
    else if (c == "resume") { rt.resumeInfoRequest = true; multimediaScreen(); }
    else if (c == "resumesave") { rt.resumeSaveRequest = true; multimediaScreen(); }
    else if (c == "resumeload") { rt.resumeLoadRequest = true; multimediaScreen(); }
    else if (c == "resumeclear") { rt.resumeClearRequest = true; multimediaScreen(); }
    else if (c == "favoriteadd") { rt.favoriteAddCurrentRequest = true; favoritesScreen(); }
    else if (c == "favoriteclear") { rt.favoriteClearRequest = true; favoritesScreen(); }
    else if (c == "favoritesave") { rt.favoriteSaveRequest = true; favoritesScreen(); }
    else if (c == "favoriteload") { rt.favoriteLoadRequest = true; favoritesScreen(); }
    else if (c == "wav" || c == "play") {
      // Play guard: do not request another RT WAV task while one is active or pending.
      if (rt.rtAudioTaskRunning || rt.audioBusy || rt.wavPlayRequest) {
        Serial.println("[ALIAS] play ignored: IQPlayerCore already active/busy");
        playerScreen();
      } else {
        // v9.1.1: only real WAV starts can show LOADING immediately.
        // Unsupported FLAC/OPUS/MP3/OGG/AAC must remain READY until decoder exists.
        rt.mediaState = (mediaCodecFromPath(rt.mediaPath) == MEDIA_CODEC_WAV) ? MEDIA_STATE_LOADING : MEDIA_STATE_READY;
        rt.wavPlayRequest = true;
        playerScreen();
      }
    }
    else if (c == "playwav") { rt.playWavFallbackRequest = true; playerScreen(); }
    else if (c.startsWith("volume ") || c.startsWith("vol ")) {
      int sp = c.indexOf(' ');
      String arg = sp >= 0 ? c.substring(sp + 1) : String("");
      arg.trim();
      bool valid = arg.length() > 0;
      for (size_t i = 0; valid && i < arg.length(); ++i) {
        if (!isDigit((unsigned char)arg[i])) valid = false;
      }
      int requested = valid ? arg.toInt() : -1;
      if (!valid || requested < 0 || requested > 100) {
        Serial.println("[VOLUME] usage: volume 0..100");
      } else {
        vol = requested;
        audio.setVolume(vol);
        rt.volumePercent = vol;
        msg = "Гучність: " + String(vol);
        Serial.printf("[VOLUME] set=%d%% source=command\n", vol);
        if (screen == SCR_PLAYER) updatePlayerVolumeOnly();
        else if (screen == SCR_AUDIO) audioScreen();
      }
    }
    else if (c == "stop") {
      if (screen == SCR_PLAYER) {
        rt.wavStopRequest = true;
        updatePlayerPartials(true);
      } else {
        Serial.println("[PLAYER] STOP ignored: open Player screen first");
      }
    }
    else if (c == "back") home();
    else if (c == "index") { rt.indexRequest = true; filesScreen(); }
    else if (c == "tone") rt.audioToneRequest = true;
    else if (c.length()) {
      Serial.print("[CMD] unknown: ");
      Serial.println(c);
      Serial.println("Type h for help");
    }
  }

  void tick() {
    rt.core1Loops++;

    // v9.5-alpha2.1: report the real UI update rate (full + partial frames),
    // not only framebuffer pushes performed by present().
    const uint32_t fpsNow = millis();
    if (fpsNow - lastFpsMs >= 1000) {
      const uint32_t total = renderer.fullFrames + renderer.partialFrames;
      fps = (uint16_t)min<uint32_t>(65535, total - lastUiFrameTotal);
      lastUiFrameTotal = total;
      lastFpsMs = fpsNow;
      frames = 0;
    }

    static uint32_t lastAsyncRedraw = 0;
    static uint32_t lastEventSeen = 0;
    if ((rt.scanLock || rt.dbScanBusy) && millis() - lastAsyncRedraw > 500) {
      lastAsyncRedraw = millis();
      scanLockScreen();
      return;
    }
    if (lastEventSeen != rt.lastCore0Event) {
      lastEventSeen = rt.lastCore0Event;
      rt.uiDirty = false;
      if (screen == SCR_WIFI) wifiScreen();
      else if (screen == SCR_SD) sdScreen();
      else if (screen == SCR_AUDIO) audioScreen();
      else if (screen == SCR_PLAYER) {
        // v8.2.5: no full-screen redraw for progress/time/VU/status events.
        updatePlayerPartials(false);
      }
      else if (screen == SCR_STATUS) status();
    }
    if ((screen == SCR_WIFI || screen == SCR_SD) && millis() - lastAsyncRedraw > 500) {
      lastAsyncRedraw = millis();
      if (screen == SCR_WIFI) wifiScreen();
      else if (screen == SCR_SD) sdScreen();
    }
    if (screen == SCR_PLAYER) updatePlayerPartials(false);

    int d = nav.delta(false, 2);
    if (d) {
      if (screen == SCR_HOME) {
        sel += d;
        if (sel < 0) sel = HOME_ITEM_COUNT - 1;
        if (sel >= HOME_ITEM_COUNT) sel = 0;
        msg = d > 0 ? "Навігація вниз" : "Навігація вгору";
        home();
      } else if (screen == SCR_ENCODERS) encoders();
      Serial.printf("NAV %d sel=%d\n", d, sel);
    }

    if (nav.pressed()) {
      if (screen == SCR_HOME) openSelected();
      else home();
    }

    int vd = volEnc.delta(false, 2); // NORMAL direction
    if (vd) {
      vol += vd * 2;
      if (vol < 0) vol = 0;
      if (vol > 100) vol = 100;
      audio.setVolume(vol);
      rt.volumePercent = vol;
      msg = "Гучність: " + String(vol);
      if (screen == SCR_HOME) home();
      else if (screen == SCR_ENCODERS) encoders();
      else if (screen == SCR_AUDIO) audioScreen();
      else if (screen == SCR_PLAYER) updatePlayerVolumeOnly();
      Serial.printf("VOL %d = %d\n", vd, vol);
    }

    if (volEnc.pressed()) {
      // v9.8-alpha43: volume encoder button is a dedicated PLAY key.
      // Rotation still controls volume. A press starts the selected/resumed
      // track and opens the Player screen without restarting active audio.
      const bool active = rt.rtAudioTaskRunning || rt.audioPlaying ||
                          rt.audioBusy || rt.wavPlayRequest;
      if (rt.scanLock || rt.dbScanBusy) {
        msg = "PLAY недоступний: сканування";
        if (screen == SCR_HOME) home();
        Serial.println("[VOL-BTN] PLAY blocked: scan active");
      } else if (active) {
        msg = "Вже відтворюється";
        playerScreen();
        Serial.println("[VOL-BTN] PLAY ignored: player active/busy");
      } else if (!rt.mediaPath[0]) {
        msg = "Немає вибраного треку";
        if (screen == SCR_HOME) home();
        Serial.println("[VOL-BTN] PLAY rejected: empty media path");
      } else {
        rt.mediaState = MEDIA_STATE_LOADING;
        rt.trackHandoffActive = true;
        rt.trackHandoffAutoPlay = true;
        rt.wavPlayRequest = true;
        rt.uiDirty = true;
        msg = "PLAY";
        playerScreen();
        Serial.printf("[VOL-BTN] PLAY requested: %s\n", rt.mediaPath);
      }
    }
  }
};
