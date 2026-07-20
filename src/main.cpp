#include <Arduino.h>
#include <Preferences.h>
#include <esp_log.h>
#define IQ200_DB_TEST 0  // v7.7: boot DB test disabled; run dbtest manually
#include "drivers/Display.h"
#include "services/SystemService.h"
#include "services/WifiService.h"
#include "services/StorageService.h"
#include "services/SDManager.h"
#include "services/ArtworkCache.h"
#include "services/AudioEngine.h"
#include "services/RuntimeState.h"
#include "services/EventQueue.h"
#include "services/WatchdogService.h"
#include "services/SchedulerService.h"
#include "services/AppManager.h"
#include "services/MessageBroker.h"
#include "services/CoreOS.h"
#include "services/OTAService.h"
#include "services/SettingsService.h"
#include "services/ServiceManager.h"
#include "services/AudioStream.h"
#include "services/FileIndex.h"
#include "services/PlaylistManager.h"
#include "services/WiFiProfiles.h"
#include "services/WavPlayerService.h"
#include "services/IQPlayerCore.h"
#include "services/FLACPlayerService.h"
#include "services/MP3PlayerService.h"
#include "services/MediaEngine.h"
#include "services/MediaFramework.h"
#include "services/MediaDatabase.h"
#include "services/QueueManager.h"
#include "services/MediaCore.h"
#include "services/EventBus.h"
#include "services/ResumeEngine.h"
#include "services/MediaPipeline.h"
#include "services/FavoriteManager.h"
#include "services/LibraryManager.h"
#include "services/ScanService.h"
#include "services/DatabaseService.h"
#include "services/SmartResumeService.h"
#include "services/ConnectivityManager.h"
#include "services/WebServerService.h"
#include "services/RadioService.h"
#include "services/StabilityService.h"
#include "services/CommercialPolishService.h"
#include "services/BlackBoxService.h"
#include "services/CommandManager.h"
#include "services/ModeManager.h"
#include "ui/UI.h"
#include "ui/ModeCenterUI.h"
#include "ui/WebRadioModeUI.h"

IQ200Display display;
SystemService systemService;
WifiService wifiService;
StorageService storageService;
AudioEngine audioEngine;
RuntimeState runtimeState;
WatchdogService watchdogService;
SchedulerService schedulerService;
AppManager appManager;
MessageBroker messageBroker;
CoreOS coreOS;
OTAService otaService;
SettingsService settingsService;
ServiceManager serviceManager;
AudioStream audioStream;
FileIndex fileIndex;
PlaylistManager playlist;
WiFiProfiles wifiProfiles;
WavPlayerService wavPlayer;
FLACPlayerService flacPlayer;
MP3PlayerService mp3Player;
IQPlayerCore iqPlayerCore;
MediaEngine mediaEngine;
MediaDatabase mediaDb;
QueueManager queueManager;
MediaCore mediaCore;
EventBus eventBus;
ResumeEngine resumeEngine;
MediaPipeline mediaPipeline;
FavoriteManager favoriteManager;
LibraryManager libraryManager;
ScanService scanService;
DatabaseService databaseService;
SmartResumeService smartResumeService;
ConnectivityManager connectivityManager;
RadioStationStore radioStationStore;
WebServerService webServerService(radioStationStore);
RadioService radioService;
StabilityService stabilityService;
CommercialPolishService commercialPolishService;
BlackBoxService blackBox;
CommandManager commandManager;
ModeManager modeManager;

UI ui(display, systemService, wifiService, storageService, audioEngine, runtimeState, appManager);
ModeCenterUI modeCenterUi(display, modeManager);
WebRadioModeUI webRadioModeUi(display, runtimeState, radioService, radioStationStore);

TaskHandle_t core0TaskHandle = nullptr;
TaskHandle_t core1TaskHandle = nullptr;
TaskHandle_t webTaskHandle = nullptr;
TaskHandle_t modeUiTaskHandle = nullptr;
static volatile bool webTaskCreated = false;
static IQ200Mode activeMode = IQ200_MODE_CENTER;
QueueHandle_t iqEventQueue = nullptr;

// v9.2-alpha15: runtime process tracing. Enabled by default for field diagnostics.
static volatile bool processLogEnabled = true;
static volatile uint32_t processLogIntervalMs = 1000;
// v9.8-alpha39: hard serial diagnostics gate. When disabled, UART output is
// physically stopped; Web commands remain available and can enable it again.
static volatile bool diagnosticsOutputEnabled = true;
static Preferences diagnosticsPrefs;

static void diagnosticsSaveEnabled(bool enabled) {
  if (diagnosticsPrefs.begin("iq200diag", false)) {
    diagnosticsPrefs.putBool("enabled", enabled);
    diagnosticsPrefs.end();
  }
}

static bool diagnosticsLoadEnabled() {
  bool enabled = true;
  if (diagnosticsPrefs.begin("iq200diag", true)) {
    enabled = diagnosticsPrefs.getBool("enabled", true);
    diagnosticsPrefs.end();
  }
  return enabled;
}

static void diagnosticsOutputSet(bool enabled, bool persist = true) {
  if (persist) diagnosticsSaveEnabled(enabled);
  processLogEnabled = enabled;
  diagnosticsOutputEnabled = enabled;
  if (enabled) {
    Serial.begin(115200);
    Serial.setTimeout(10);
    esp_log_level_set("*", ESP_LOG_WARN);
    delay(20);
    Serial.println("[DIAG] full diagnostics output ENABLED");
  } else {
    Serial.println("[DIAG] full diagnostics output DISABLED; enable from Web Console: diagnostics on");
    Serial.flush();
    esp_log_level_set("*", ESP_LOG_NONE);
    Serial.end();
  }
}

static const char* processEventName(IQEventType type) {
  switch (type) {
    case EVT_WIFI_DONE: return "WIFI_DONE";
    case EVT_SD_DONE: return "SD_DONE";
    case EVT_AUDIO_DONE: return "AUDIO_DONE";
    case EVT_INDEX_DONE: return "INDEX_DONE";
    case EVT_AUDIO_STREAM_DONE: return "AUDIO_STREAM_DONE";
    case EVT_WAV_OPENED: return "WAV_OPENED";
    case EVT_WAV_STARTED: return "WAV_STARTED";
    case EVT_WAV_PROGRESS: return "WAV_PROGRESS";
    case EVT_WAV_STOPPED: return "WAV_STOPPED";
    case EVT_MONITOR: return "MONITOR";
    case EVT_ERROR: return "ERROR";
    default: return "NONE";
  }
}

static void processLogStatus() {
  Serial.printf("[PROC][CFG] enabled=%d interval=%lums core=%d task=%s queue=%u/%u\n",
                processLogEnabled ? 1 : 0,
                (unsigned long)processLogIntervalMs,
                xPortGetCoreID(), pcTaskGetName(nullptr),
                iqEventQueue ? (unsigned)uxQueueMessagesWaiting(iqEventQueue) : 0U,
                iqEventQueue ? 12U : 0U);
  SDManager::printStats(Serial);
}

static void processLogCore0() {
  static uint32_t lastMs = 0;
  static uint32_t lastLoops = 0;
  const uint32_t now = millis();
  if (!processLogEnabled || now - lastMs < processLogIntervalMs) return;
  const uint32_t elapsed = now - lastMs;
  const uint32_t loopDelta = runtimeState.core0Loops - lastLoops;
  const uint32_t loopRate = elapsed ? (loopDelta * 1000UL) / elapsed : 0;
  lastLoops = runtimeState.core0Loops;
  lastMs = now;
  Serial.printf("[PROC][C0] t=%lu task=%s loops/s=%lu heap=%lu stackHW=%lu state=%s codec=%u play=%d rt=%d busy=%d nextReq=%d nextAuto=%d progress=%u%% health=%u underrun=%lu short=%lu handoff=%s(%d) pl=%d/%d scan=%d/%d evPost=%lu evDrop=%lu evCoal=%lu q=%u\n",
                (unsigned long)now, pcTaskGetName(nullptr), (unsigned long)loopRate,
                (unsigned long)runtimeState.core0HeapFree,
                (unsigned long)runtimeState.core0StackHighWater,
                runtimeState.playerStateName,
                (unsigned)runtimeState.mediaCodec,
                runtimeState.audioPlaying ? 1 : 0,
                runtimeState.rtAudioTaskRunning ? 1 : 0,
                runtimeState.audioBusy ? 1 : 0,
                runtimeState.playlistNextRequest ? 1 : 0,
                runtimeState.playlistNextAutoPlayRequest ? 1 : 0,
                (unsigned)runtimeState.mediaProgress,
                (unsigned)runtimeState.audioHealth,
                (unsigned long)runtimeState.audioUnderruns,
                (unsigned long)runtimeState.audioShortWrites,
                runtimeState.trackHandoffState,
                runtimeState.trackHandoffActive ? 1 : 0,
                runtimeState.playlistCount ? runtimeState.playlistIndex + 1 : 0,
                runtimeState.playlistCount,
                runtimeState.dbScanBusy ? 1 : 0,
                (unsigned)runtimeState.scanProgress,
                (unsigned long)runtimeState.eventBusPosts,
                (unsigned long)runtimeState.eventBusDrops,
                (unsigned long)messageBroker.coalesced(),
                iqEventQueue ? (unsigned)uxQueueMessagesWaiting(iqEventQueue) : 0U);
  static uint8_t sdDivider = 0;
  if (++sdDivider >= 5) {
    sdDivider = 0;
    SDManager::printStats(Serial);
  }
}

static void processLogCore1() {
  static uint32_t lastMs = 0;
  static uint32_t lastLoops = 0;
  static uint32_t lastFrames = 0;
  static uint32_t lastVuTicks = 0, lastVuDraws = 0;
  static uint32_t lastProgTicks = 0, lastProgDraws = 0, lastMetaDraws = 0;
  const uint32_t now = millis();
  const uint32_t elapsed = now - lastMs;
  if (!processLogEnabled || elapsed < processLogIntervalMs) return;
  const uint32_t loopDelta = runtimeState.core1Loops - lastLoops;
  const uint32_t frameDelta = runtimeState.rendererFrames - lastFrames;
  const uint32_t vuTickDelta = runtimeState.playerVuTicks - lastVuTicks;
  const uint32_t vuDrawDelta = runtimeState.playerVuDraws - lastVuDraws;
  const uint32_t progTickDelta = runtimeState.playerProgressTicks - lastProgTicks;
  const uint32_t progDrawDelta = runtimeState.playerProgressDraws - lastProgDraws;
  const uint32_t metaDrawDelta = runtimeState.playerMetaDraws - lastMetaDraws;
  const uint32_t loopRate = elapsed ? (loopDelta * 1000UL) / elapsed : 0;
  const uint32_t frameRate = elapsed ? (frameDelta * 1000UL) / elapsed : 0;
  const uint32_t vuTickRate = elapsed ? (vuTickDelta * 1000UL) / elapsed : 0;
  const uint32_t vuDrawRate = elapsed ? (vuDrawDelta * 1000UL) / elapsed : 0;
  const uint32_t progTickRate = elapsed ? (progTickDelta * 1000UL) / elapsed : 0;
  const uint32_t progDrawRate = elapsed ? (progDrawDelta * 1000UL) / elapsed : 0;
  const uint32_t metaDrawRate = elapsed ? (metaDrawDelta * 1000UL) / elapsed : 0;
  lastLoops = runtimeState.core1Loops;
  lastFrames = runtimeState.rendererFrames;
  lastVuTicks = runtimeState.playerVuTicks;
  lastVuDraws = runtimeState.playerVuDraws;
  lastProgTicks = runtimeState.playerProgressTicks;
  lastProgDraws = runtimeState.playerProgressDraws;
  lastMetaDraws = runtimeState.playerMetaDraws;
  lastMs = now;
  Serial.printf("[PROC][C1] t=%lu task=%s loops/s=%lu frames/s=%lu vu=%lu/%lu prog=%lu/%lu meta=%lu heap=%lu stackHW=%lu app=%s dirty=%d full=%lu partial=%lu events=%lu q=%u wd=%d/%d age=%lu/%lu\n",
                (unsigned long)now, pcTaskGetName(nullptr),
                (unsigned long)loopRate, (unsigned long)frameRate,
                (unsigned long)vuTickRate, (unsigned long)vuDrawRate,
                (unsigned long)progTickRate, (unsigned long)progDrawRate,
                (unsigned long)metaDrawRate,
                (unsigned long)runtimeState.core1HeapFree,
                (unsigned long)runtimeState.core1StackHighWater,
                runtimeState.currentApp,
                runtimeState.uiDirty ? 1 : 0,
                (unsigned long)runtimeState.fullFrames,
                (unsigned long)runtimeState.partialFrames,
                (unsigned long)runtimeState.lastCore0Event,
                iqEventQueue ? (unsigned)uxQueueMessagesWaiting(iqEventQueue) : 0U,
                runtimeState.core0Ok ? 1 : 0, runtimeState.core1Ok ? 1 : 0,
                (unsigned long)runtimeState.core0AgeMs,
                (unsigned long)runtimeState.core1AgeMs);
}

static void sendEvent(IQEventType type, int value, uint64_t value64, const char* msg) {
  if (type != EVT_WAV_PROGRESS) {
    blackBox.record(type == EVT_ERROR ? BlackBoxService::KIND_ERROR : BlackBoxService::KIND_EVENT,
                    processEventName(type), value, value64, msg);
  }
  const bool ok = eventBus.post(type, value, value64, msg);
  if (!ok) runtimeState.eventQueueDrops++;
  if (processLogEnabled && type != EVT_WAV_PROGRESS) {
    Serial.printf("[PROC][EVT+] core=%d task=%s type=%s value=%d value64=%llu ok=%d msg=%s\n",
                  xPortGetCoreID(), pcTaskGetName(nullptr), processEventName(type), value,
                  (unsigned long long)value64, ok ? 1 : 0, msg ? msg : "");
  }
}

static const char* kAutoplayCfgPath = "/iq200/db/resume/autoplay.cfg";

static bool autoplayEnsureDir() {
  if (!storageService.ok) storageService.mount();
  databaseService.startIfMounted();
  if (!storageService.ok) return false;
  if (!SD.exists("/iq200")) SD.mkdir("/iq200");
  if (!SD.exists("/iq200/db")) SD.mkdir("/iq200/db");
  if (!SD.exists("/iq200/db/resume")) SD.mkdir("/iq200/db/resume");
  return true;
}

static bool autoplayLoadConfig() {
  runtimeState.autoplayEnabled = false;
  strncpy(runtimeState.autoplayStatus, "OFF", sizeof(runtimeState.autoplayStatus) - 1);
  runtimeState.autoplayStatus[sizeof(runtimeState.autoplayStatus) - 1] = 0;
  if (!autoplayEnsureDir()) return false;
  if (!SD.exists(kAutoplayCfgPath)) return false;
  File f = SD.open(kAutoplayCfgPath, FILE_READ);
  if (!f) return false;
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.startsWith("ENABLED=")) {
      runtimeState.autoplayEnabled = line.substring(8).toInt() != 0;
    } else if (line.startsWith("DELAY_MS=")) {
      uint32_t d = (uint32_t)line.substring(9).toInt();
      if (d >= 500 && d <= 15000) runtimeState.autoplayDelayMs = d;
    }
  }
  f.close();
  strncpy(runtimeState.autoplayStatus, runtimeState.autoplayEnabled ? "ON" : "OFF", sizeof(runtimeState.autoplayStatus) - 1);
  runtimeState.autoplayStatus[sizeof(runtimeState.autoplayStatus) - 1] = 0;
  return true;
}

static bool autoplaySaveConfig() {
  if (!autoplayEnsureDir()) return false;
  File f = SD.open(kAutoplayCfgPath, FILE_WRITE);
  if (!f) return false;
  f.println("# IQ200 AUTOPLAY v1");
  f.printf("ENABLED=%d\n", runtimeState.autoplayEnabled ? 1 : 0);
  f.printf("DELAY_MS=%lu\n", (unsigned long)runtimeState.autoplayDelayMs);
  f.close();
  strncpy(runtimeState.autoplayStatus, runtimeState.autoplayEnabled ? "ON" : "OFF", sizeof(runtimeState.autoplayStatus) - 1);
  runtimeState.autoplayStatus[sizeof(runtimeState.autoplayStatus) - 1] = 0;
  return true;
}

static void autoplayPrintStatus() {
  Serial.printf("[AUTOPLAY] enabled=%d pending=%d tried=%d starts=%lu skips=%lu retries=%lu/%lu delay=%lums status=%s track=%s\n",
                runtimeState.autoplayEnabled ? 1 : 0, runtimeState.autoplayPending ? 1 : 0,
                runtimeState.autoplayTried ? 1 : 0, (unsigned long)runtimeState.autoplayStartCount,
                (unsigned long)runtimeState.autoplaySkipCount, (unsigned long)runtimeState.autoplayRetryCount,
                (unsigned long)runtimeState.autoplayMaxRetries, (unsigned long)runtimeState.autoplayDelayMs,
                runtimeState.autoplayStatus, runtimeState.mediaPath);
}

static void autoplayReschedule(const char* status, uint32_t delayMs) {
  runtimeState.autoplayRetryCount++;
  runtimeState.autoplayPending = true;
  runtimeState.autoplayTried = false;
  runtimeState.autoplayReadyMs = millis() + delayMs;
  runtimeState.autoplayLastDecisionMs = millis();
  strncpy(runtimeState.autoplayStatus, status ? status : "WAIT", sizeof(runtimeState.autoplayStatus) - 1);
  runtimeState.autoplayStatus[sizeof(runtimeState.autoplayStatus) - 1] = 0;
  Serial.printf("[AUTOPLAY] retry %lu/%lu status=%s next=%lums track=%s\n",
                (unsigned long)runtimeState.autoplayRetryCount,
                (unsigned long)runtimeState.autoplayMaxRetries,
                runtimeState.autoplayStatus, (unsigned long)delayMs, runtimeState.mediaPath);
}

static void scanLockBegin(const char* msg) {
  // v7.4.1: hard reset scan counters before every scan/rescan.
  runtimeState.scanLock = true;
  runtimeState.dbScanBusy = true;
  runtimeState.scanProgress = 0;
  runtimeState.scanFiles = 0;
  runtimeState.scanTracks = 0;
  runtimeState.scanDirs = 0;
  runtimeState.scanMp3 = 0;
  runtimeState.scanFlac = 0;
  runtimeState.scanWav = 0;
  runtimeState.scanStartMs = millis();
  runtimeState.scanElapsedMs = 0;
  strncpy(runtimeState.scanCurrentPath, "Waiting...", sizeof(runtimeState.scanCurrentPath) - 1);
  runtimeState.scanCurrentPath[sizeof(runtimeState.scanCurrentPath) - 1] = 0;
  strncpy(runtimeState.scanMessage, msg, sizeof(runtimeState.scanMessage) - 1);
  runtimeState.scanMessage[sizeof(runtimeState.scanMessage) - 1] = 0;
  strncpy(runtimeState.lastMessage, msg, sizeof(runtimeState.lastMessage) - 1);
  runtimeState.lastMessage[sizeof(runtimeState.lastMessage) - 1] = 0;
  runtimeState.uiDirty = true;
  Serial.println("========================================");
  Serial.println("[SCAN] SD media scan started");
  Serial.println("[SCAN] All media/process commands are locked until scan is complete");
  Serial.println("========================================");
  sendEvent(EVT_MONITOR, 0, 0, msg);
}

static void scanLockUpdateElapsed() {
  if (runtimeState.scanLock) {
    runtimeState.scanElapsedMs = millis() - runtimeState.scanStartMs;
  }
}

static void scanLockEnd(const char* msg) {
  runtimeState.scanProgress = 100;
  runtimeState.scanElapsedMs = millis() - runtimeState.scanStartMs;
  strncpy(runtimeState.scanMessage, msg, sizeof(runtimeState.scanMessage) - 1);
  runtimeState.scanMessage[sizeof(runtimeState.scanMessage) - 1] = 0;
  strncpy(runtimeState.lastMessage, msg, sizeof(runtimeState.lastMessage) - 1);
  runtimeState.lastMessage[sizeof(runtimeState.lastMessage) - 1] = 0;
  runtimeState.scanLock = false;
  runtimeState.dbScanBusy = false;
  runtimeState.uiDirty = true;
  Serial.println("========================================");
  Serial.printf("[SCAN] complete: tracks=%d volumes=%d art=%d time=%lu ms\n",
    runtimeState.dbTrackCount, runtimeState.dbVolumeCount, runtimeState.dbArtCount,
    (unsigned long)runtimeState.scanElapsedMs);
  Serial.println("========================================");
  sendEvent(EVT_MONITOR, runtimeState.dbTrackCount, runtimeState.dbVolumeCount, msg);
}


static bool isSupportedMediaPath(const String& path) {
  String p = path;
  p.toLowerCase();
  return p.endsWith(".flac") || p.endsWith(".opus") || p.endsWith(".mp3") || p.endsWith(".ogg") || p.endsWith(".oga") || p.endsWith(".wav") || p.endsWith(".aac") || p.endsWith(".m4a");
}

static bool pathHasExt(const String& path, const char* ext) {
  String p = path;
  p.toLowerCase();
  return p.endsWith(ext);
}

static int mediaPriority(const String& path) {
  // v7.6.0: DATABASE ENGINE + single-pass scan + SD-backed media.meta summary.
  if (pathHasExt(path, ".flac")) return 0;
  if (pathHasExt(path, ".opus")) return 1;
  if (pathHasExt(path, ".mp3")) return 2;
  if (pathHasExt(path, ".ogg") || pathHasExt(path, ".oga")) return 3;
  if (pathHasExt(path, ".wav")) return 4;
  if (pathHasExt(path, ".aac") || pathHasExt(path, ".m4a")) return 5;
  return 9;
}

static const char* codecNameFromCodec(MediaCodec codec) {
  switch (codec) {
    case MEDIA_CODEC_WAV: return "WAV";
    case MEDIA_CODEC_MP3: return "MP3";
    case MEDIA_CODEC_FLAC: return "FLAC";
    case MEDIA_CODEC_OPUS: return "OPUS";
    case MEDIA_CODEC_OGG: return "OGG";
    case MEDIA_CODEC_AAC: return "AAC";
    case MEDIA_CODEC_RADIO: return "RADIO";
    default: return "NONE";
  }
}

static String normalizeRootPath(const String& name) {
  if (!name.length()) return String("");
  if (name[0] == '/') return name;
  return String("/") + name;
}

static void setMediaPathFromPlaylist(const String& path) {
  if (!path.length()) return;
  strncpy(runtimeState.mediaPath, path.c_str(), sizeof(runtimeState.mediaPath) - 1);
  runtimeState.mediaPath[sizeof(runtimeState.mediaPath) - 1] = 0;
  strncpy(runtimeState.playlistCurrent, path.c_str(), sizeof(runtimeState.playlistCurrent) - 1);
  runtimeState.playlistCurrent[sizeof(runtimeState.playlistCurrent) - 1] = 0;
  const char* title = mediaTitleFromPath(runtimeState.mediaPath);
  strncpy(runtimeState.mediaTitle, title, sizeof(runtimeState.mediaTitle) - 1);
  runtimeState.mediaTitle[sizeof(runtimeState.mediaTitle) - 1] = 0;
  runtimeState.mediaCodec = mediaCodecFromPath(runtimeState.mediaPath);
  runtimeState.playlistCount = playlist.size();
  runtimeState.playlistIndex = playlist.index();
}

// Synchronize the actual PlaylistManager cursor with resume.dat. The saved
// index is the fast path; PATH remains authoritative after a database reorder.
static bool restorePlaylistPositionFromResume(const char* reason) {
  if (!runtimeState.resumeRestored || playlist.size() <= 0) return false;

  char savedPath[sizeof(runtimeState.mediaPath)];
  strncpy(savedPath, runtimeState.mediaPath, sizeof(savedPath) - 1);
  savedPath[sizeof(savedPath) - 1] = 0;
  const int savedIndex = runtimeState.resumeLoadedPlaylistIndex;
  int resolvedIndex = -1;
  const char* source = "NONE";

  if (savedIndex >= 0 && savedIndex < playlist.size() && playlist.at(savedIndex).equals(savedPath)) {
    resolvedIndex = savedIndex;
    source = "INDEX";
  }
  if (resolvedIndex < 0 && savedPath[0]) {
    for (int i = 0; i < playlist.size(); ++i) {
      if (playlist.at(i).equals(savedPath)) {
        resolvedIndex = i;
        source = "PATH";
        break;
      }
      if ((i & 63) == 63) delay(0);
    }
  }
  if (resolvedIndex < 0 && savedIndex >= 0 && savedIndex < playlist.size()) {
    resolvedIndex = savedIndex;
    source = "INDEX_FALLBACK";
  }
  if (resolvedIndex < 0) {
    resolvedIndex = 0;
    source = "FIRST_FALLBACK";
  }
  if (!playlist.select(resolvedIndex)) return false;
  setMediaPathFromPlaylist(playlist.currentTrack());
  Serial.printf("[SMARTRESUME][PL] %s source=%s saved=%d/%d restored=%d/%d path=%s\n",
                reason ? reason : "RESTORE", source,
                savedIndex >= 0 ? savedIndex + 1 : 0,
                runtimeState.resumeLoadedPlaylistCount,
                playlist.index() + 1, playlist.size(), runtimeState.mediaPath);
  return true;
}

static void setHandoffState(const char* state) {
  strncpy(runtimeState.trackHandoffState, state ? state : "", sizeof(runtimeState.trackHandoffState) - 1);
  runtimeState.trackHandoffState[sizeof(runtimeState.trackHandoffState) - 1] = 0;
}

static bool atomicPlaylistSwitch(bool forward, bool& suppressRtCompletionOnce, bool forceAutoPlay = false) {
  if (playlist.size() <= 0) return false;

  const bool wasPlaying = iqPlayerCore.isPlaying() || iqPlayerCore.isTaskRunning() ||
                          runtimeState.audioPlaying || runtimeState.rtAudioTaskRunning;
  runtimeState.trackHandoffActive = true;
  const bool shouldAutoPlay = wasPlaying || forceAutoPlay;
  runtimeState.trackHandoffAutoPlay = shouldAutoPlay;
  runtimeState.trackHandoffLastMs = millis();
  runtimeState.trackHandoffCount++;
  runtimeState.audioBusy = true;
  runtimeState.mediaState = MEDIA_STATE_LOADING;
  setHandoffState("STOP_CURRENT");
  strncpy(runtimeState.playerStateName, "HANDOFF", sizeof(runtimeState.playerStateName) - 1);
  runtimeState.playerStateName[sizeof(runtimeState.playerStateName) - 1] = 0;
  runtimeState.uiDirty = true;

  if (wasPlaying) {
    // v9.2-alpha16: an EOF-driven handoff may still observe the old RT task
    // during its final teardown. That stale task state must not arm the
    // one-shot suppression flag, otherwise the NEXT track EOF is mistaken
    // for a manual STOP and Auto Next runs only once.
    const bool activeAudioBeforeStop = iqPlayerCore.isPlaying() || runtimeState.audioPlaying;
    const bool armCompletionSuppress = !forceAutoPlay && activeAudioBeforeStop;
    suppressRtCompletionOnce = armCompletionSuppress;
    Serial.printf("[HANDOFF][FSM] enter source=%s wasPlaying=%d activeAudio=%d rtTask=%d suppress=%d state=%s\n",
                  forceAutoPlay ? "EOF" : "USER",
                  wasPlaying ? 1 : 0,
                  activeAudioBeforeStop ? 1 : 0,
                  iqPlayerCore.isTaskRunning() ? 1 : 0,
                  suppressRtCompletionOnce ? 1 : 0,
                  runtimeState.playerStateName);
    Serial.printf("[HANDOFF] %s: stopping current decoder\n", forward ? "NEXT" : "PREV");
    if (!iqPlayerCore.stopAndWait(1500)) {
      runtimeState.trackHandoffTimeouts++;
      runtimeState.trackHandoffActive = false;
      runtimeState.trackHandoffAutoPlay = false;
      runtimeState.audioBusy = false;
      runtimeState.mediaState = MEDIA_STATE_ERROR;
      setHandoffState("STOP_TIMEOUT");
      strncpy(runtimeState.playerStateName, "ERROR", sizeof(runtimeState.playerStateName) - 1);
      Serial.printf("[HANDOFF][ERROR] decoder stop timeout count=%lu\n",
                    (unsigned long)runtimeState.trackHandoffTimeouts);
      return false;
    }
    // v9.2-alpha9: give the SD/FATFS stack time to finish the previous
    // close/status transaction before opening the next FLAC file. Rapid
    // close/open cycles were observed to trigger cmd 0x0d on some cards.
    vTaskDelay(pdMS_TO_TICKS(150));
    if (forceAutoPlay) {
      // EOF completion was already consumed before this deferred handoff.
      suppressRtCompletionOnce = false;
      Serial.println("[HANDOFF][FSM] EOF teardown complete; suppress cleared");
    }
  }

  setHandoffState("SELECT_TRACK");
  const String& selected = forward ? playlist.next() : playlist.prev();
  setMediaPathFromPlaylist(selected);
  runtimeState.mediaProgress = 0;
  runtimeState.mediaPlayedBytes = 0;
  runtimeState.wavProgress = 0;
  runtimeState.wavPlayedBytes = 0;
  runtimeState.mediaVuLeft = 0;
  runtimeState.mediaVuRight = 0;
  runtimeState.audioBusy = false;

  if (shouldAutoPlay) {
    setHandoffState("START_NEW");
    runtimeState.mediaState = MEDIA_STATE_LOADING;
    runtimeState.wavPlayRequest = true;
  } else {
    runtimeState.mediaState = MEDIA_STATE_READY;
    runtimeState.trackHandoffActive = false;
    runtimeState.trackHandoffAutoPlay = false;
    setHandoffState("SELECTED");
    strncpy(runtimeState.playerStateName, "READY", sizeof(runtimeState.playerStateName) - 1);
  }
  runtimeState.uiDirty = true;
  return true;
}

static bool selectNextTrackByCodec(MediaCodec wanted) {
  if (playlist.size() <= 0) return false;
  int start = playlist.index();
  for (int step = 0; step < playlist.size(); ++step) {
    int idx = (start + step) % playlist.size();
    const String& candidate = playlist.at(idx);
    if (mediaCodecFromPath(candidate.c_str()) == wanted) {
      while (playlist.index() != idx) playlist.next();
      setMediaPathFromPlaylist(candidate);
      return true;
    }
  }
  return false;
}

static void mirrorActiveMode() {
  runtimeState.systemMode = static_cast<uint8_t>(activeMode);
  runtimeState.modeEarlyBootFailures = modeManager.failureCount();
  runtimeState.modeBootHealthy = modeManager.isHealthy();
  runtimeState.modeStartedMs = millis();
  strncpy(runtimeState.systemModeName, ModeManager::name(activeMode), sizeof(runtimeState.systemModeName) - 1);
  runtimeState.systemModeName[sizeof(runtimeState.systemModeName) - 1] = 0;
}

static bool handleModeCommand(const String& command) {
  if (command == "mode" || command == "mode status") {
    Serial.printf("[MODE] active=%s id=%u healthy=%d earlyFailures=%u\n",
                  runtimeState.systemModeName, (unsigned)runtimeState.systemMode,
                  runtimeState.modeBootHealthy ? 1 : 0,
                  (unsigned)runtimeState.modeEarlyBootFailures);
    Serial.println("[MODE] switch: mode center | mode local | mode webradio");
    Serial.println("[MODE] reserved: mode bluetooth | mode radio");
    return true;
  }
  if (!command.startsWith("mode ")) return false;
  const IQ200Mode requested = ModeManager::parse(command.substring(5));
  if (static_cast<uint8_t>(requested) == 255U) {
    Serial.println("[MODE] invalid. Use center/local/webradio/bluetooth/radio");
    return true;
  }
  if (!ModeManager::available(requested)) {
    Serial.printf("[MODE] %s reserved for future hardware/driver\n", ModeManager::name(requested));
    return true;
  }
  runtimeState.modeSwitchRequest = static_cast<int8_t>(requested);
  Serial.printf("[MODE] clean reboot requested -> %s\n", ModeManager::name(requested));
  return true;
}

static void modeHealthTick() {
  static uint32_t lastAttemptMs = 0;
  const uint32_t now = millis();
  if (runtimeState.modeBootHealthy || now - runtimeState.modeStartedMs < 10000U) return;
  // A failed NVS write must not be retried at the 5-10 ms task-loop rate.
  if (lastAttemptMs && now - lastAttemptMs < 5000U) return;
  lastAttemptMs = now;
  runtimeState.modeBootHealthy = modeManager.markHealthy();
  runtimeState.modeEarlyBootFailures = modeManager.failureCount();
}

static void processNetworkRequests() {
  if (runtimeState.netApRequest) {
    runtimeState.netApRequest = false;
    connectivityManager.startAp("IQ200-OS");
  }
  if (runtimeState.wifiStaConnectRequest) {
    runtimeState.wifiStaConnectRequest = false;
    connectivityManager.startSta(runtimeState.wifiPendingSsid, runtimeState.wifiPendingPassword, false, true);
  }
  if (runtimeState.wifiApStaConnectRequest) {
    runtimeState.wifiApStaConnectRequest = false;
    connectivityManager.startApSta(runtimeState.wifiPendingSsid, runtimeState.wifiPendingPassword);
  }
  if (runtimeState.wifiLoadRequest) {
    runtimeState.wifiLoadRequest = false;
    connectivityManager.loadAndConnect(false);
  }
  if (runtimeState.wifiSaveRequest) {
    runtimeState.wifiSaveRequest = false;
    const bool ok = connectivityManager.saveCredentials(runtimeState.wifiPendingSsid, runtimeState.wifiPendingPassword);
    Serial.printf("[NET] profile save=%d ssid='%s'\n", ok ? 1 : 0, runtimeState.wifiPendingSsid);
  }
  if (runtimeState.wifiDisconnectRequest) {
    runtimeState.wifiDisconnectRequest = false;
    connectivityManager.disconnectSta(false);
  }
  if (runtimeState.wifiScanNowRequest) {
    runtimeState.wifiScanNowRequest = false;
    connectivityManager.scan();
  }
  if (runtimeState.wifiStatusRequest) {
    runtimeState.wifiStatusRequest = false;
    connectivityManager.print();
  }
  if (runtimeState.wifiForgetRequest) {
    runtimeState.wifiForgetRequest = false;
    connectivityManager.disconnectSta(false);
    connectivityManager.forgetCredentials();
  }
  if (runtimeState.wifiAutoOnRequest) { runtimeState.wifiAutoOnRequest = false; connectivityManager.setAutoConnect(true); }
  if (runtimeState.wifiAutoOffRequest) { runtimeState.wifiAutoOffRequest = false; connectivityManager.setAutoConnect(false); }
  if (runtimeState.wifiFallbackOnRequest) { runtimeState.wifiFallbackOnRequest = false; connectivityManager.setFallbackAp(true); }
  if (runtimeState.wifiFallbackOffRequest) { runtimeState.wifiFallbackOffRequest = false; connectivityManager.setFallbackAp(false); }
  if (runtimeState.wifiBootRequest) { runtimeState.wifiBootRequest = false; connectivityManager.boot(); }
  if (runtimeState.netOffRequest) {
    runtimeState.netOffRequest = false;
    webServerService.enable(false);
    connectivityManager.stop();
  }
  if (runtimeState.webEnableRequest) {
    runtimeState.webEnableRequest = false;
    webServerService.enable(true);
  }
  if (runtimeState.webDisableRequest) {
    runtimeState.webDisableRequest = false;
    webServerService.enable(false);
  }
  if (runtimeState.webInfoRequest) {
    runtimeState.webInfoRequest = false;
    connectivityManager.print();
    webServerService.print();
  }
}

static void processModeSwitchRequest() {
  const int requestedValue = runtimeState.modeSwitchRequest;
  if (requestedValue < 0) return;
  runtimeState.modeSwitchRequest = -1;
  const IQ200Mode target = static_cast<IQ200Mode>(requestedValue);
  if (!ModeManager::available(target)) return;
  if (target == activeMode) {
    Serial.printf("[MODE] already active: %s\n", ModeManager::name(target));
    return;
  }
  if (!modeManager.setNext(target)) {
    Serial.println("[MODE][ERROR] NVS save failed; reboot cancelled");
    return;
  }

  Serial.printf("[MODE] clean shutdown %s -> %s\n", ModeManager::name(activeMode), ModeManager::name(target));
  if (activeMode == IQ200_MODE_CENTER) {
    // Mode Center deliberately has no Web, WiFi, SD or audio platform to stop.
    Serial.println("[MODE] center is clean; rebooting into selected platform");
    Serial.flush();
    delay(50);
    ESP.restart();
    return;
  }
  // Let the HTTP 202 response leave the TCP buffer before Web is stopped.
  vTaskDelay(pdMS_TO_TICKS(150));
  if (activeMode == IQ200_MODE_LOCAL_PLAYER) {
    iqPlayerCore.stopAndWait(1800);
    runtimeState.audioPlaying = false;
    runtimeState.rtAudioTaskRunning = false;
    resumeEngine.save();
    audioEngine.stop();
  } else if (activeMode == IQ200_MODE_WEBRADIO) {
    radioService.stop();
    const uint32_t stopStarted = millis();
    while (radioService.isActive() && millis() - stopStarted < 500U) {
      vTaskDelay(pdMS_TO_TICKS(10));
    }
    audioEngine.stop();
  }
  webServerService.enable(false);
  connectivityManager.stop();
  Serial.println("[MODE] platform cleared; rebooting");
  Serial.flush();
  delay(50);
  ESP.restart();
}

static bool requestCenterOnDualHold() {
  static uint32_t bothPressedSince = 0;
  const bool bothPressed = digitalRead(IQ200_ENC_NAV_SW) == LOW &&
                           digitalRead(IQ200_ENC_VOL_SW) == LOW;
  if (!bothPressed) {
    bothPressedSince = 0;
    return false;
  }
  if (!bothPressedSince) bothPressedSince = millis();
  if (millis() - bothPressedSince >= 2000U) {
    bothPressedSince = millis() + 60000U;
    runtimeState.modeSwitchRequest = static_cast<int8_t>(IQ200_MODE_CENTER);
    Serial.println("[MODE] dual encoder hold -> MODE_CENTER");
  }
  return true;
}

void core0Worker(void* parameter) {
  Serial.printf("[TASK] core0Worker running on core %d\n", xPortGetCoreID());
  // v9.1.2: distinguish user/handoff STOP from decoder EOF. Without this,
  // prev/next/stop can be misreported as RT finished 100% and poison resume.
  static bool suppressRtCompletionOnce = false;

  for (;;) {
    runtimeState.core0Loops++;
    watchdogService.beatCore0();
    // alpha6 normally owns HTTP from webServiceTask. Keep a zero-cost fallback
    // here so Web remains available if its dedicated task could not be created.
    if (!webTaskCreated) webServerService.tick();
    modeHealthTick();
    processModeSwitchRequest();
    serviceManager.tick();
    mediaPipeline.tick();
    runtimeState.serviceTicks = serviceManager.status().ticks;
    runtimeState.core0HeapFree = ESP.getFreeHeap();
    runtimeState.core0StackHighWater = uxTaskGetStackHighWaterMark(nullptr);
    schedulerService.tickAudio();
    schedulerService.tickSD();
    schedulerService.tickWiFi();
    schedulerService.tickOTA();
    schedulerService.tickNVS();
    runtimeState.audioTicks = schedulerService.stats.audioTicks;
    runtimeState.sdTicks = schedulerService.stats.sdTicks;
    runtimeState.wifiTicks = schedulerService.stats.wifiTicks;

    static bool bootDbLoadTried = false;
    if (!bootDbLoadTried && millis() > 3000) {
      bootDbLoadTried = true;
      if (!storageService.ok) storageService.mount();
      databaseService.startIfMounted();
      // v8.2.5: MediaDatabase touches /iq200/db on SD. Do not call begin()
  // until StorageService has successfully mounted the SD card.
  runtimeState.bootReadyMs = millis();
  strncpy(runtimeState.bootPhase, "INIT", sizeof(runtimeState.bootPhase) - 1);
  runtimeState.bootPhase[sizeof(runtimeState.bootPhase) - 1] = 0;
      int metaTracks = 0, metaVolumes = 0, metaArt = 0, metaMp3 = 0, metaFlac = 0, metaWav = 0, metaFiles = 0;
      if (mediaDb.readManifest(&metaTracks, &metaVolumes, &metaArt, &metaMp3, &metaFlac, &metaWav, &metaFiles)) {
        runtimeState.dbTrackCount = metaTracks;
        runtimeState.dbVolumeCount = metaVolumes;
        runtimeState.dbArtCount = metaArt;
        runtimeState.dbMp3Count = metaMp3;
        runtimeState.dbFlacCount = metaFlac;
        runtimeState.dbWavCount = metaWav;
        Serial.printf("[DBMETA] loaded: tracks=%d volumes=%d art=%d mp3=%d flac=%d wav=%d\n",
                      metaTracks, metaVolumes, metaArt, metaMp3, metaFlac, metaWav);
      }
      bool fastOk = mediaDb.validateManifestFast(true);
      if (fastOk && mediaDb.exists()) {
        Serial.println("[DBFAST] valid database found; full SD scan skipped");
        int loaded = mediaDb.loadPlaylist(playlist);
        if (playlist.size() > 0) setMediaPathFromPlaylist(playlist.currentTrack());
        runtimeState.playlistCount = playlist.size();
        runtimeState.playlistIndex = playlist.index();
        Serial.printf("[DB] boot load: active=%d current=%d/%d %s\n", loaded, playlist.size() ? playlist.index() + 1 : 0, playlist.size(), runtimeState.mediaPath);
        bool resumed = smartResumeService.restoreAtBoot();
        if (resumed) {
          audioEngine.setEqualizer(runtimeState.eqEnabled, runtimeState.eqBassDb, runtimeState.eqMidDb, runtimeState.eqTrebleDb, runtimeState.eqPreset);
          String qcur = queueManager.current();
          if (qcur.length()) {
            strncpy(runtimeState.queueCurrent, qcur.c_str(), sizeof(runtimeState.queueCurrent) - 1);
            runtimeState.queueCurrent[sizeof(runtimeState.queueCurrent) - 1] = 0;
          }
          if (runtimeState.resumeRestored) restorePlaylistPositionFromResume("BOOT");
          mediaCore.mirrorQueue();
        }
        // v9.3-alpha2.4: safe startup volume. Resume restores the track and
        // repeat state, but every power-on starts at 8% to prevent a loud boot.
        audioEngine.setVolume(8);
        runtimeState.volumePercent = 8;
        settingsService.setVolume(8);
        Serial.println("[AUDIO] boot volume forced to 8%");
        autoplayLoadConfig();
        if (runtimeState.autoplayEnabled && runtimeState.mediaPath[0]) {
          runtimeState.autoplayPending = true;
          runtimeState.autoplayTried = false;
          runtimeState.autoplayRetryCount = 0;
          runtimeState.autoplayLastDecisionMs = millis();
          runtimeState.autoplayReadyMs = millis() + runtimeState.autoplayDelayMs;
          strncpy(runtimeState.autoplayStatus, "WAIT_BOOT", sizeof(runtimeState.autoplayStatus) - 1);
          runtimeState.autoplayStatus[sizeof(runtimeState.autoplayStatus) - 1] = 0;
          Serial.printf("[AUTOPLAY] armed: delay=%lums track=%s\n", (unsigned long)runtimeState.autoplayDelayMs, runtimeState.mediaPath);
        } else {
          Serial.printf("[AUTOPLAY] %s track=%s\n", runtimeState.autoplayEnabled ? "skip:no-track" : "off", runtimeState.mediaPath);
        }
#if IQ200_DB_TEST
        Serial.println("[DBTEST] boot check enabled (temporary)");
        runtimeState.dbTestErrors = mediaDb.selfTest(true);
#endif
        sendEvent(EVT_MONITOR, loaded, 0, "DB boot load");
      } else {
        Serial.println("[DBFAST] no valid SD media database yet. Use scan/rescan to build /iq200/db/media/media_000.db");
      }
    }

    scanService.tick();
    smartResumeService.tick();
    connectivityManager.tick();
    radioService.tick();
    stabilityService.tick();
    commercialPolishService.tick();

    if (runtimeState.autoplayOnRequest) {
      runtimeState.autoplayOnRequest = false;
      runtimeState.autoplayEnabled = true;
      runtimeState.autoplayTried = false;
      runtimeState.autoplayRetryCount = 0;
      runtimeState.autoplayLastDecisionMs = millis();
      bool ok = autoplaySaveConfig();
      Serial.printf("[AUTOPLAY] ON save=%d delay=%lums\n", ok ? 1 : 0, (unsigned long)runtimeState.autoplayDelayMs);
      sendEvent(EVT_MONITOR, ok ? 1 : 0, runtimeState.autoplayDelayMs, ok ? "Autoplay on" : "Autoplay save fail");
    }
    if (runtimeState.autoplayOffRequest) {
      runtimeState.autoplayOffRequest = false;
      runtimeState.autoplayEnabled = false;
      runtimeState.autoplayPending = false;
      bool ok = autoplaySaveConfig();
      Serial.printf("[AUTOPLAY] OFF save=%d\n", ok ? 1 : 0);
      sendEvent(EVT_MONITOR, ok ? 1 : 0, 0, ok ? "Autoplay off" : "Autoplay save fail");
    }
    if (runtimeState.autoplayInfoRequest) {
      runtimeState.autoplayInfoRequest = false;
      autoplayLoadConfig();
      autoplayPrintStatus();
      sendEvent(EVT_MONITOR, runtimeState.autoplayEnabled ? 1 : 0, runtimeState.autoplayStartCount, "Autoplay status");
    }
    if (runtimeState.autoplayPending && !runtimeState.autoplayTried && (int32_t)(millis() - runtimeState.autoplayReadyMs) >= 0) {
      runtimeState.autoplayLastDecisionMs = millis();
      if (!runtimeState.autoplayEnabled) {
        runtimeState.autoplayTried = true;
        runtimeState.autoplayPending = false;
        runtimeState.autoplaySkipCount++;
        strncpy(runtimeState.autoplayStatus, "OFF", sizeof(runtimeState.autoplayStatus) - 1);
      } else if (!storageService.ok || !runtimeState.sdOk || runtimeState.dbTrackCount <= 0 || !runtimeState.mediaPath[0]) {
        if (runtimeState.autoplayRetryCount < runtimeState.autoplayMaxRetries) {
          autoplayReschedule("WAIT_SD_DB", 1000);
        } else {
          runtimeState.autoplayTried = true;
          runtimeState.autoplayPending = false;
          runtimeState.autoplaySkipCount++;
          strncpy(runtimeState.autoplayStatus, "SKIP_NOT_READY", sizeof(runtimeState.autoplayStatus) - 1);
          Serial.printf("[AUTOPLAY] skip:not-ready sd=%d db=%d track=%s\n", runtimeState.sdOk ? 1 : 0, runtimeState.dbTrackCount, runtimeState.mediaPath);
        }
      } else if (!SD.exists(runtimeState.mediaPath)) {
        runtimeState.autoplayTried = true;
        runtimeState.autoplayPending = false;
        runtimeState.autoplaySkipCount++;
        strncpy(runtimeState.autoplayStatus, "SKIP_MISSING", sizeof(runtimeState.autoplayStatus) - 1);
        Serial.printf("[AUTOPLAY] skip:missing %s\n", runtimeState.mediaPath);
      } else if (runtimeState.rtAudioTaskRunning || runtimeState.audioPlaying || runtimeState.audioBusy || runtimeState.wavPlayRequest) {
        if (runtimeState.autoplayRetryCount < runtimeState.autoplayMaxRetries) {
          autoplayReschedule("WAIT_BUSY", 750);
        } else {
          runtimeState.autoplayTried = true;
          runtimeState.autoplayPending = false;
          runtimeState.autoplaySkipCount++;
          strncpy(runtimeState.autoplayStatus, "SKIP_BUSY", sizeof(runtimeState.autoplayStatus) - 1);
          Serial.println("[AUTOPLAY] skip: player busy");
        }
      } else {
        runtimeState.autoplayTried = true;
        runtimeState.autoplayPending = false;
        runtimeState.mediaPlayedBytes = 0;
        runtimeState.mediaProgress = 0;
        runtimeState.playWavTestMode = false;
        runtimeState.wavPlayRequest = true;
        runtimeState.autoplayStartCount++;
        strncpy(runtimeState.autoplayStatus, "START", sizeof(runtimeState.autoplayStatus) - 1);
        Serial.printf("[AUTOPLAY] start track-only from 0%%: %s\n", runtimeState.mediaPath);
        sendEvent(EVT_MONITOR, 1, runtimeState.autoplayStartCount, "Autoplay start");
      }
      runtimeState.autoplayStatus[sizeof(runtimeState.autoplayStatus) - 1] = 0;
    }

    if (runtimeState.netApRequest) {
      runtimeState.netApRequest = false;
      connectivityManager.startAp("IQ200-OS");
    }
    if (runtimeState.wifiStaConnectRequest) {
      runtimeState.wifiStaConnectRequest = false;
      connectivityManager.startSta(runtimeState.wifiPendingSsid, runtimeState.wifiPendingPassword, false, true);
    }
    if (runtimeState.wifiApStaConnectRequest) {
      runtimeState.wifiApStaConnectRequest = false;
      connectivityManager.startApSta(runtimeState.wifiPendingSsid, runtimeState.wifiPendingPassword);
    }
    if (runtimeState.wifiLoadRequest) {
      runtimeState.wifiLoadRequest = false;
      connectivityManager.loadAndConnect(false);
    }
    if (runtimeState.wifiSaveRequest) {
      runtimeState.wifiSaveRequest = false;
      const bool ok = connectivityManager.saveCredentials(runtimeState.wifiPendingSsid, runtimeState.wifiPendingPassword);
      Serial.printf("[NET] profile save=%d ssid='%s'\n", ok ? 1 : 0, runtimeState.wifiPendingSsid);
    }
    if (runtimeState.wifiDisconnectRequest) {
      runtimeState.wifiDisconnectRequest = false;
      connectivityManager.disconnectSta(false);
    }
    if (runtimeState.wifiScanNowRequest) {
      runtimeState.wifiScanNowRequest = false;
      connectivityManager.scan();
    }
    if (runtimeState.wifiStatusRequest) {
      runtimeState.wifiStatusRequest = false;
      connectivityManager.print();
    }
    if (runtimeState.wifiForgetRequest) {
      runtimeState.wifiForgetRequest = false;
      connectivityManager.disconnectSta(false);
      connectivityManager.forgetCredentials();
    }
    if (runtimeState.wifiAutoOnRequest) { runtimeState.wifiAutoOnRequest = false; connectivityManager.setAutoConnect(true); }
    if (runtimeState.wifiAutoOffRequest) { runtimeState.wifiAutoOffRequest = false; connectivityManager.setAutoConnect(false); }
    if (runtimeState.wifiFallbackOnRequest) { runtimeState.wifiFallbackOnRequest = false; connectivityManager.setFallbackAp(true); }
    if (runtimeState.wifiFallbackOffRequest) { runtimeState.wifiFallbackOffRequest = false; connectivityManager.setFallbackAp(false); }
    if (runtimeState.wifiBootRequest) { runtimeState.wifiBootRequest = false; connectivityManager.boot(); }
    if (runtimeState.netOffRequest) {
      runtimeState.netOffRequest = false;
      webServerService.enable(false);
      connectivityManager.stop();
    }
    if (runtimeState.webEnableRequest) {
      runtimeState.webEnableRequest = false;
      webServerService.enable(true);
      Serial.println("[WEB] enable requested. Use net ap first if no WiFi is connected.");
    }
    if (runtimeState.webDisableRequest) {
      runtimeState.webDisableRequest = false;
      webServerService.enable(false);
    }
    if (runtimeState.webInfoRequest) {
      runtimeState.webInfoRequest = false;
      connectivityManager.print();
      webServerService.print();
    }
    if (runtimeState.otaInfoRequest) {
      runtimeState.otaInfoRequest = false;
      Serial.printf("[OTA] status=%s busy=%d progress=%d%% sd_file=/iq200/firmware/iq200.bin\n", runtimeState.otaStatus, runtimeState.otaBusy ? 1 : 0, runtimeState.otaProgress);
    }
    if (runtimeState.otaSdRequest) {
      runtimeState.otaSdRequest = false;
      Serial.println("[OTA] SD OTA requested: validation hook ready. Full flash update will be enabled after burn-in.");
      strncpy(runtimeState.otaStatus, "SD_READY", sizeof(runtimeState.otaStatus)-1);
      runtimeState.otaStatus[sizeof(runtimeState.otaStatus)-1] = 0;
    }
    if (runtimeState.radioInfoRequest) {
      runtimeState.radioInfoRequest = false;
      radioService.print();
    }
    if (runtimeState.radioStopRequest) {
      runtimeState.radioStopRequest = false;
      radioService.stop();
    }
    if (runtimeState.stabilityInfoRequest) {
      runtimeState.stabilityInfoRequest = false;
      stabilityService.print();
    }
    if (runtimeState.commercialInfoRequest) {
      runtimeState.commercialInfoRequest = false;
      commercialPolishService.print();
    }
    if (runtimeState.burninStartRequest) {
      runtimeState.burninStartRequest = false;
      stabilityService.setBurnin(true);
      Serial.println("[BURNIN] started. Use burnin stop to stop.");
    }
    if (runtimeState.burninStopRequest) {
      runtimeState.burninStopRequest = false;
      stabilityService.setBurnin(false);
      Serial.println("[BURNIN] stopped.");
    }

    serviceManager.setScannerBusy(scanService.busy());
    if (runtimeState.scanLock) {
      // During SD database rebuild, keep Core0 alive but do not start other jobs.
      runtimeState.wifiScanRequest = false;
      runtimeState.sdMountRequest = false;
      runtimeState.indexRequest = false;
      runtimeState.playlistScanRequest = false;
      runtimeState.dbLoadRequest = false;
      runtimeState.dbInfoRequest = false;
      runtimeState.dbClearRequest = false;
      runtimeState.playlistNextRequest = false;
      runtimeState.playlistPrevRequest = false;
      runtimeState.playlistAddTestRequest = false;
      runtimeState.playlistClearRequest = false;
      runtimeState.queueListRequest = false;
      runtimeState.queueAddCurrentRequest = false;
      runtimeState.queueClearRequest = false;
      runtimeState.queueNextRequest = false;
      runtimeState.queuePrevRequest = false;
      runtimeState.queueSaveRequest = false;
      runtimeState.queueLoadRequest = false;
      runtimeState.queueShuffleToggleRequest = false;
      runtimeState.queueModeInfoRequest = false;
      runtimeState.queueRepeatSetRequest = -1;
      runtimeState.favoriteListRequest = false;
      runtimeState.favoriteAddCurrentRequest = false;
      runtimeState.favoriteClearRequest = false;
      runtimeState.favoriteSaveRequest = false;
      runtimeState.favoriteLoadRequest = false;
      runtimeState.wavPlayRequest = false;
      runtimeState.audioToneRequest = false;
    }

    if (runtimeState.wifiScanRequest && !runtimeState.wifiScanBusy) {
      runtimeState.wifiScanRequest = false;
      runtimeState.wifiScanBusy = true;
      Serial.println("[CORE0] WiFi scan begin");
      int n = wifiService.scan();
      runtimeState.wifiNetworks = n;
      sendEvent(EVT_WIFI_DONE, n, 0, "WiFi scan done");
      runtimeState.wifiScanBusy = false;
      Serial.printf("[CORE0] WiFi scan done: %d\n", n);
    }

    if (runtimeState.artworkReloadRequest && !runtimeState.audioBusy && !runtimeState.audioPlaying && !runtimeState.rtAudioTaskRunning) {
      runtimeState.artworkReloadRequest = false;
      const char* artPath = runtimeState.mediaPath[0] ? runtimeState.mediaPath : runtimeState.playlistCurrent;
      if (artPath && artPath[0]) {
        if (!storageService.ok) storageService.mount();
        bool ok = ArtworkCache::instance().prepareForTrack(artPath, true);
        Serial.printf("[ART] reload done ok=%d track=%s\n", ok ? 1 : 0, artPath);
        sendEvent(EVT_MONITOR, ok ? 1 : 0, ArtworkCache::instance().size(), ok ? "Artwork reloaded" : "Artwork fallback");
      } else {
        Serial.println("[ART] reload skipped: no current track");
      }
    }

    if (runtimeState.sdMountRequest && !runtimeState.sdBusy) {
      runtimeState.sdMountRequest = false;
      runtimeState.sdBusy = true;
      Serial.println("[CORE0] SD mount begin");
      bool ok = storageService.mount();
      runtimeState.sdOk = ok;
      runtimeState.sdMB = storageService.mb;
      sendEvent(EVT_SD_DONE, ok ? 1 : 0, storageService.mb, ok ? "SD OK" : "SD FAIL");
      runtimeState.sdBusy = false;
      Serial.printf("[CORE0] SD mount done: %d MB=%llu\n", ok, storageService.mb);
    }


    if (runtimeState.indexRequest && !runtimeState.indexBusy) {
      runtimeState.indexRequest = false;
      runtimeState.indexBusy = true;
      Serial.println("[CORE0] File index begin");
      if (!storageService.ok) storageService.mount();
      int n = fileIndex.scanRoot();
      runtimeState.fileIndexCount = n;
      sendEvent(EVT_INDEX_DONE, n, 0, "Index done");
      runtimeState.indexBusy = false;
      Serial.printf("[CORE0] File index done: %d\n", n);
    }



    // v7.3 Media Framework: lightweight playlist commands.
    if (runtimeState.playlistClearRequest) {
      runtimeState.playlistClearRequest = false;
      playlist.clear();
      runtimeState.playlistCount = 0;
      runtimeState.playlistIndex = 0;
      strncpy(runtimeState.playlistCurrent, "", sizeof(runtimeState.playlistCurrent));
      Serial.println("[CORE0] Playlist cleared");
      sendEvent(EVT_MONITOR, 0, 0, "Playlist cleared");
    }

    if (runtimeState.playlistAddTestRequest) {
      runtimeState.playlistAddTestRequest = false;
      bool ok = playlist.add("/test.wav");
      setMediaPathFromPlaylist(playlist.currentTrack());
      Serial.printf("[CORE0] Playlist add /test.wav: %d count=%d\n", ok, playlist.size());
      sendEvent(EVT_MONITOR, playlist.size(), 0, ok ? "Playlist add" : "Playlist full");
    }



    // v8.0.4 Smart Queue modes: shuffle and repeat are handled on Core0.
    if (runtimeState.queueShuffleToggleRequest) {
      runtimeState.queueShuffleToggleRequest = false;
      queueManager.toggleShuffleSmart();
      smartResumeService.markDirty();
      runtimeState.queueShuffleSmart = queueManager.shuffleSmart();
      runtimeState.queueRepeatMode = queueManager.repeatMode();
      Serial.printf("[QUEUE] smart shuffle: %s repeat=%s count=%d\n",
                    queueManager.shuffleSmart() ? "ON" : "OFF",
                    queueManager.repeatName(), queueManager.size());
      sendEvent(EVT_MONITOR, queueManager.shuffleSmart() ? 1 : 0, queueManager.repeatMode(), "Queue shuffle");
    }

    if (runtimeState.queueRepeatSetRequest >= 0) {
      int mode = runtimeState.queueRepeatSetRequest;
      runtimeState.queueRepeatSetRequest = -1;
      queueManager.setRepeatMode(mode);
      smartResumeService.markDirty();
      runtimeState.queueShuffleSmart = queueManager.shuffleSmart();
      runtimeState.queueRepeatMode = queueManager.repeatMode();
      Serial.printf("[QUEUE] repeat: %s shuffle=%s count=%d\n",
                    queueManager.repeatName(),
                    queueManager.shuffleSmart() ? "ON" : "OFF", queueManager.size());
      sendEvent(EVT_MONITOR, queueManager.repeatMode(), queueManager.shuffleSmart() ? 1 : 0, "Queue repeat");
    }

    if (runtimeState.queueModeInfoRequest) {
      runtimeState.queueModeInfoRequest = false;
      runtimeState.queueShuffleSmart = queueManager.shuffleSmart();
      runtimeState.queueRepeatMode = queueManager.repeatMode();
      Serial.printf("[QUEUE] mode: shuffle=%s repeat=%s current=%d/%d\n",
                    queueManager.shuffleSmart() ? "ON" : "OFF",
                    queueManager.repeatName(), queueManager.index() + 1, queueManager.size());
      sendEvent(EVT_MONITOR, queueManager.size(), queueManager.repeatMode(), "Queue mode");
    }

    // v8.0.1 Media Core QueueManager: non-audio queue foundation.
    if (runtimeState.queueAddCurrentRequest) {
      runtimeState.queueAddCurrentRequest = false;
      String p = String(runtimeState.mediaPath[0] ? runtimeState.mediaPath : runtimeState.playlistCurrent);
      bool ok = queueManager.add(p);
      smartResumeService.markDirty();
      mediaCore.mirrorQueue();
      String cur = queueManager.current();
      strncpy(runtimeState.queueCurrent, cur.c_str(), sizeof(runtimeState.queueCurrent) - 1);
      runtimeState.queueCurrent[sizeof(runtimeState.queueCurrent) - 1] = 0;
      runtimeState.queueShuffleSmart = queueManager.shuffleSmart();
      runtimeState.queueRepeatMode = queueManager.repeatMode();
      Serial.printf("[QUEUE] add current: %d count=%d shuffle=%s repeat=%s current=%s\n", ok, queueManager.size(), queueManager.shuffleSmart() ? "ON" : "OFF", queueManager.repeatName(), runtimeState.queueCurrent);
      sendEvent(EVT_MONITOR, queueManager.size(), queueManager.index(), ok ? "Queue add" : "Queue full");
    }

    if (runtimeState.queueClearRequest) {
      runtimeState.queueClearRequest = false;
      queueManager.clear();
      smartResumeService.markDirty();
      mediaCore.mirrorQueue();
      strncpy(runtimeState.queueCurrent, "", sizeof(runtimeState.queueCurrent));
      Serial.println("[QUEUE] cleared");
      sendEvent(EVT_MONITOR, 0, 0, "Queue cleared");
    }

    if (runtimeState.queueNextRequest) {
      runtimeState.queueNextRequest = false;
      String p = queueManager.next();
      smartResumeService.markDirty();
      mediaCore.queueCurrentToRuntime();
      strncpy(runtimeState.queueCurrent, p.c_str(), sizeof(runtimeState.queueCurrent) - 1);
      runtimeState.queueCurrent[sizeof(runtimeState.queueCurrent) - 1] = 0;
      if (p.length()) setMediaPathFromPlaylist(p);
      runtimeState.queueShuffleSmart = queueManager.shuffleSmart();
      runtimeState.queueRepeatMode = queueManager.repeatMode();
      if (p.length()) Serial.printf("[QUEUE] next: %d/%d [%s/%s] %s\n", queueManager.index() + 1, queueManager.size(), queueManager.shuffleSmart() ? "SHUF" : "SEQ", queueManager.repeatName(), p.c_str());
      else Serial.printf("[QUEUE] next: end of queue [%s/%s]\n", queueManager.shuffleSmart() ? "SHUF" : "SEQ", queueManager.repeatName());
      sendEvent(EVT_MONITOR, queueManager.index(), queueManager.size(), "Queue next");
    }

    if (runtimeState.queuePrevRequest) {
      runtimeState.queuePrevRequest = false;
      String p = queueManager.prev();
      smartResumeService.markDirty();
      mediaCore.queueCurrentToRuntime();
      strncpy(runtimeState.queueCurrent, p.c_str(), sizeof(runtimeState.queueCurrent) - 1);
      runtimeState.queueCurrent[sizeof(runtimeState.queueCurrent) - 1] = 0;
      if (p.length()) setMediaPathFromPlaylist(p);
      runtimeState.queueShuffleSmart = queueManager.shuffleSmart();
      runtimeState.queueRepeatMode = queueManager.repeatMode();
      Serial.printf("[QUEUE] prev: %d/%d [%s/%s] %s\n", queueManager.index() + 1, queueManager.size(), queueManager.shuffleSmart() ? "SHUF" : "SEQ", queueManager.repeatName(), p.c_str());
      sendEvent(EVT_MONITOR, queueManager.index(), queueManager.size(), "Queue prev");
    }

    if (runtimeState.queueSaveRequest) {
      runtimeState.queueSaveRequest = false;
      if (!storageService.ok) storageService.mount();
      bool ok = databaseService.saveQueue();
      Serial.printf("[QUEUE] save /iq200/db/queue/queue.db: %d count=%d shuffle=%s repeat=%s\n", ok, queueManager.size(), queueManager.shuffleSmart() ? "ON" : "OFF", queueManager.repeatName());
      sendEvent(EVT_MONITOR, ok ? 1 : 0, queueManager.size(), ok ? "Queue saved" : "Queue save fail");
    }

    if (runtimeState.queueLoadRequest) {
      runtimeState.queueLoadRequest = false;
      if (!storageService.ok) storageService.mount();
      bool ok = databaseService.loadQueue();
      mediaCore.mirrorQueue();
      String cur = queueManager.current();
      strncpy(runtimeState.queueCurrent, cur.c_str(), sizeof(runtimeState.queueCurrent) - 1);
      runtimeState.queueCurrent[sizeof(runtimeState.queueCurrent) - 1] = 0;
      if (cur.length()) setMediaPathFromPlaylist(cur);
      Serial.printf("[QUEUE] load /iq200/db/queue/queue.db: %d count=%d shuffle=%s repeat=%s current=%s\n", ok, queueManager.size(), queueManager.shuffleSmart() ? "ON" : "OFF", queueManager.repeatName(), runtimeState.queueCurrent);
      sendEvent(EVT_MONITOR, ok ? queueManager.size() : 0, queueManager.index(), ok ? "Queue loaded" : "Queue load fail");
    }

    if (runtimeState.queueListRequest) {
      runtimeState.queueListRequest = false;
      mediaCore.mirrorQueue();
      runtimeState.queueShuffleSmart = queueManager.shuffleSmart();
      runtimeState.queueRepeatMode = queueManager.repeatMode();
      Serial.printf("[QUEUE] count=%d current=%d shuffle=%s repeat=%s\n", queueManager.size(), queueManager.index() + 1, queueManager.shuffleSmart() ? "ON" : "OFF", queueManager.repeatName());
      if (queueManager.size() == 0) {
        Serial.println("[QUEUE] empty. Use qadd to add current track.");
      } else {
        int maxPrint = queueManager.size();
        if (maxPrint > 32) maxPrint = 32;
        for (int i = 0; i < maxPrint; i++) {
          MediaCodec cdc = mediaCodecFromPath(queueManager.at(i).c_str());
          Serial.printf("%c %02d [%s] %s\n", i == queueManager.index() ? '>' : ' ', i + 1, codecNameFromCodec(cdc), queueManager.at(i).c_str());
        }
      }
      sendEvent(EVT_MONITOR, queueManager.size(), queueManager.index(), "Queue list");
    }


    // v8.0.5 Media Pipeline diagnostics.
    if (runtimeState.pipelineInfoRequest) {
      runtimeState.pipelineInfoRequest = false;
      mediaPipeline.print();
      sendEvent(EVT_MONITOR, runtimeState.pipelineFileQueueDepth, runtimeState.pipelineDropped, "Pipeline info");
    }


    // v8.1.5 Favorites: SD-backed favorites.db foundation.
    if (runtimeState.favoriteLoadRequest) {
      runtimeState.favoriteLoadRequest = false;
      if (!storageService.ok) storageService.mount();
      databaseService.startIfMounted();
      uint32_t t0 = millis();
      bool ok = databaseService.loadFavorites();
      runtimeState.favoriteCount = favoriteManager.size();
      runtimeState.favoriteLastMs = millis() - t0;
      Serial.printf("[FAV] load %s: %d count=%d time=%lu ms\n", favoriteManager.path(), ok ? 1 : 0, favoriteManager.size(), (unsigned long)runtimeState.favoriteLastMs);
      sendEvent(EVT_MONITOR, favoriteManager.size(), ok ? 1 : 0, ok ? "Favorites loaded" : "Favorites load fail");
    }

    if (runtimeState.favoriteSaveRequest) {
      runtimeState.favoriteSaveRequest = false;
      if (!storageService.ok) storageService.mount();
      databaseService.startIfMounted();
      uint32_t t0 = millis();
      bool ok = databaseService.saveFavorites();
      runtimeState.favoriteCount = favoriteManager.size();
      runtimeState.favoriteLastMs = millis() - t0;
      Serial.printf("[FAV] save %s: %d count=%d time=%lu ms\n", favoriteManager.path(), ok ? 1 : 0, favoriteManager.size(), (unsigned long)runtimeState.favoriteLastMs);
      sendEvent(EVT_MONITOR, favoriteManager.size(), ok ? 1 : 0, ok ? "Favorites saved" : "Favorites save fail");
    }

    if (runtimeState.favoriteAddCurrentRequest) {
      runtimeState.favoriteAddCurrentRequest = false;
      if (!storageService.ok) storageService.mount();
      databaseService.startIfMounted();
      favoriteManager.load();
      String p = String(runtimeState.mediaPath[0] ? runtimeState.mediaPath : runtimeState.playlistCurrent);
      bool ok = favoriteManager.add(p);
      bool saved = favoriteManager.save();
      runtimeState.favoriteCount = favoriteManager.size();
      strncpy(runtimeState.favoriteLastPath, p.c_str(), sizeof(runtimeState.favoriteLastPath) - 1);
      runtimeState.favoriteLastPath[sizeof(runtimeState.favoriteLastPath) - 1] = 0;
      runtimeState.favoriteLastMs = 0;
      Serial.printf("[FAV] add current: ok=%d save=%d count=%d path=%s\n", ok ? 1 : 0, saved ? 1 : 0, favoriteManager.size(), p.c_str());
      sendEvent(EVT_MONITOR, favoriteManager.size(), ok ? 1 : 0, ok ? "Favorite added" : "Favorite add fail");
    }

    if (runtimeState.favoriteClearRequest) {
      runtimeState.favoriteClearRequest = false;
      if (!storageService.ok) storageService.mount();
      databaseService.startIfMounted();
      bool ok = favoriteManager.removeDb();
      runtimeState.favoriteCount = 0;
      strncpy(runtimeState.favoriteLastPath, "", sizeof(runtimeState.favoriteLastPath));
      Serial.printf("[FAV] clear %s: %d\n", favoriteManager.path(), ok ? 1 : 0);
      sendEvent(EVT_MONITOR, ok ? 1 : 0, 0, ok ? "Favorites cleared" : "Favorites clear fail");
    }

    if (runtimeState.favoriteListRequest) {
      runtimeState.favoriteListRequest = false;
      if (!storageService.ok) storageService.mount();
      databaseService.startIfMounted();
      bool ok = favoriteManager.load();
      runtimeState.favoriteCount = favoriteManager.size();
      Serial.printf("[FAV] count=%d file=%s load=%d\n", favoriteManager.size(), favoriteManager.path(), ok ? 1 : 0);
      if (favoriteManager.size() == 0) {
        Serial.println("[FAV] empty. Use favadd to add current track.");
      } else {
        int maxPrint = favoriteManager.size();
        if (maxPrint > 32) maxPrint = 32;
        for (int i = 0; i < maxPrint; i++) {
          MediaCodec cdc = mediaCodecFromPath(favoriteManager.at(i).c_str());
          Serial.printf("%c %02d [%s] %s\n", i == 0 ? '>' : ' ', i + 1, codecNameFromCodec(cdc), favoriteManager.at(i).c_str());
        }
      }
      sendEvent(EVT_MONITOR, favoriteManager.size(), ok ? 1 : 0, "Favorites list");
    }


    // v8.1.6 Media Library Complete: build and print SD-backed category indexes.
    if (runtimeState.libraryBuildRequest) {
      runtimeState.libraryBuildRequest = false;
      if (!storageService.ok) storageService.mount();
      databaseService.startIfMounted();
      const bool forceBuild = runtimeState.libraryBuildForce;
      runtimeState.libraryBuildForce = false;
      uint32_t t0 = millis();
      bool ok = databaseService.buildLibrary(forceBuild);
      runtimeState.libraryArtistCount = libraryManager.artistsTotal();
      runtimeState.libraryAlbumCount = libraryManager.albumsTotal();
      runtimeState.libraryGenreCount = libraryManager.genresTotal();
      runtimeState.libraryFolderCount = libraryManager.foldersTotal();
      runtimeState.libraryLastMs = millis() - t0;
      strncpy(runtimeState.libraryLastView, "Build", sizeof(runtimeState.libraryLastView) - 1);
      runtimeState.libraryLastView[sizeof(runtimeState.libraryLastView) - 1] = 0;
      Serial.printf("[LIB] build ok=%d mode=%s tracks=%d artists=%d albums=%d genres=%d folders=%d time=%lu ms\n",
                    ok ? 1 : 0, libraryManager.buildWasSkipped() ? "UP_TO_DATE" : "REBUILT",
                    libraryManager.tracksTotal(), runtimeState.libraryArtistCount, runtimeState.libraryAlbumCount,
                    runtimeState.libraryGenreCount, runtimeState.libraryFolderCount, (unsigned long)runtimeState.libraryLastMs);
      sendEvent(EVT_MONITOR, ok ? 1 : 0, runtimeState.libraryArtistCount,
                ok ? (libraryManager.buildWasSkipped() ? "Library up-to-date" : "Library rebuilt") : "Library build fail");
    }

    if (runtimeState.libraryStatsRequest) {
      runtimeState.libraryStatsRequest = false;
      if (!storageService.ok) storageService.mount();
      databaseService.startIfMounted();
      libraryManager.printStats();
      strncpy(runtimeState.libraryLastView, "Stats", sizeof(runtimeState.libraryLastView) - 1);
      runtimeState.libraryLastView[sizeof(runtimeState.libraryLastView) - 1] = 0;
      sendEvent(EVT_MONITOR, runtimeState.libraryArtistCount, runtimeState.libraryAlbumCount, "Library stats");
    }

    if (runtimeState.artistListRequest) {
      runtimeState.artistListRequest = false;
      if (!storageService.ok) storageService.mount();
      databaseService.startIfMounted();
      libraryManager.printList("/iq200/db/library/artist.idx", "ARTIST", 32);
      strncpy(runtimeState.libraryLastView, "Artists", sizeof(runtimeState.libraryLastView) - 1);
      runtimeState.libraryLastView[sizeof(runtimeState.libraryLastView) - 1] = 0;
      sendEvent(EVT_MONITOR, runtimeState.libraryArtistCount, 0, "Artists list");
    }

    if (runtimeState.albumListRequest) {
      runtimeState.albumListRequest = false;
      if (!storageService.ok) storageService.mount();
      databaseService.startIfMounted();
      libraryManager.printList("/iq200/db/library/album.idx", "ALBUM", 32);
      strncpy(runtimeState.libraryLastView, "Albums", sizeof(runtimeState.libraryLastView) - 1);
      runtimeState.libraryLastView[sizeof(runtimeState.libraryLastView) - 1] = 0;
      sendEvent(EVT_MONITOR, runtimeState.libraryAlbumCount, 0, "Albums list");
    }

    if (runtimeState.genreListRequest) {
      runtimeState.genreListRequest = false;
      if (!storageService.ok) storageService.mount();
      databaseService.startIfMounted();
      libraryManager.printList("/iq200/db/library/genre.idx", "GENRE", 32);
      strncpy(runtimeState.libraryLastView, "Genres", sizeof(runtimeState.libraryLastView) - 1);
      runtimeState.libraryLastView[sizeof(runtimeState.libraryLastView) - 1] = 0;
      sendEvent(EVT_MONITOR, runtimeState.libraryGenreCount, 0, "Genres list");
    }

    if (runtimeState.folderListRequest) {
      runtimeState.folderListRequest = false;
      if (!storageService.ok) storageService.mount();
      databaseService.startIfMounted();
      libraryManager.printList("/iq200/db/library/folder.idx", "FOLDER", 32);
      strncpy(runtimeState.libraryLastView, "Folders", sizeof(runtimeState.libraryLastView) - 1);
      runtimeState.libraryLastView[sizeof(runtimeState.libraryLastView) - 1] = 0;
      sendEvent(EVT_MONITOR, runtimeState.libraryFolderCount, 0, "Folders list");
    }

    if (runtimeState.recentListRequest) {
      runtimeState.recentListRequest = false;
      if (!storageService.ok) storageService.mount();
      databaseService.startIfMounted();
      libraryManager.printList("/iq200/db/library/recent.db", "RECENT", 32);
      strncpy(runtimeState.libraryLastView, "Recent", sizeof(runtimeState.libraryLastView) - 1);
      runtimeState.libraryLastView[sizeof(runtimeState.libraryLastView) - 1] = 0;
      sendEvent(EVT_MONITOR, runtimeState.libraryRecentCount, 0, "Recent list");
    }

    if (runtimeState.mostListRequest) {
      runtimeState.mostListRequest = false;
      if (!storageService.ok) storageService.mount();
      databaseService.startIfMounted();
      libraryManager.printList("/iq200/db/library/mostplayed.db", "MOST", 32);
      strncpy(runtimeState.libraryLastView, "Most", sizeof(runtimeState.libraryLastView) - 1);
      runtimeState.libraryLastView[sizeof(runtimeState.libraryLastView) - 1] = 0;
      sendEvent(EVT_MONITOR, runtimeState.libraryMostCount, 0, "Most list");
    }

    // v8.0.2 Resume Engine: explicit save/load/info/clear commands.
    if (runtimeState.resumeSaveRequest) {
      runtimeState.resumeSaveRequest = false;
      if (!storageService.ok) storageService.mount();
      databaseService.startIfMounted();
      bool ok = smartResumeService.saveNow();
      Serial.printf("[RESUME] smart save: %d path=%s volume=%d queue=%d/%d\n", ok, runtimeState.mediaPath, audioEngine.getVolume(), queueManager.index() + 1, queueManager.size());
      sendEvent(EVT_MONITOR, ok ? 1 : 0, runtimeState.resumeSaveCount, ok ? "Resume saved" : "Resume save fail");
    }

    if (runtimeState.resumeLoadRequest) {
      runtimeState.resumeLoadRequest = false;
      if (!storageService.ok) storageService.mount();
      databaseService.startIfMounted();
      bool ok = smartResumeService.restoreAtBoot();
      if (runtimeState.resumeRestored) restorePlaylistPositionFromResume("MANUAL_LOAD");
      Serial.printf("[RESUME] smart load: %d path=%s volume=%d queue=%d/%d\n", ok, runtimeState.mediaPath, audioEngine.getVolume(), queueManager.index() + 1, queueManager.size());
      sendEvent(EVT_MONITOR, ok ? 1 : 0, 0, ok ? "Resume loaded" : "Resume load fail");
    }

    if (runtimeState.resumeInfoRequest) {
      runtimeState.resumeInfoRequest = false;
      if (!storageService.ok) storageService.mount();
      databaseService.startIfMounted();
      resumeEngine.print();
      smartResumeService.print();
      sendEvent(EVT_MONITOR, runtimeState.resumeSaveCount, runtimeState.resumeLastSaveMs, "Resume info");
    }

    if (runtimeState.resumeClearRequest) {
      runtimeState.resumeClearRequest = false;
      if (!storageService.ok) storageService.mount();
      databaseService.startIfMounted();
      bool ok = resumeEngine.clear("/iq200/db/resume/resume.dat");
      strncpy(runtimeState.resumeLastPath, "", sizeof(runtimeState.resumeLastPath));
      Serial.printf("[RESUME] clear: %d\n", ok);
      sendEvent(EVT_MONITOR, ok ? 1 : 0, 0, ok ? "Resume cleared" : "Resume clear fail");
    }

    if (runtimeState.dbClearRequest) {
      runtimeState.dbClearRequest = false;
      if (!storageService.ok) storageService.mount();
      databaseService.startIfMounted();
      mediaDb.clearDbFiles();
      runtimeState.dbTrackCount = 0;
      runtimeState.dbVolumeCount = 0;
      runtimeState.dbArtCount = 0;
      Serial.println("[DB] cleared /iq200/db media/art volumes");
      sendEvent(EVT_MONITOR, 0, 0, "DB cleared");
    }

    if (runtimeState.dbInfoRequest) {
      runtimeState.dbInfoRequest = false;
      if (!storageService.ok) storageService.mount();
      databaseService.startIfMounted();
      Serial.printf("[DB] exists=%d tracks=%d volumes=%d art=%d testErrors=%d path=/iq200/db/media/media_000.db\n",
        mediaDb.exists(), runtimeState.dbTrackCount, runtimeState.dbVolumeCount, runtimeState.dbArtCount, runtimeState.dbTestErrors);
      databaseService.print();
      mediaDb.printManifest();
      mediaDb.printIndex();
      sendEvent(EVT_MONITOR, runtimeState.dbTrackCount, runtimeState.dbVolumeCount, "DB info");
    }

    if (runtimeState.dbTestRequest) {
      runtimeState.dbTestRequest = false;
      if (!storageService.ok) storageService.mount();
      databaseService.startIfMounted();
      runtimeState.dbTestErrors = mediaDb.selfTest(true);
      Serial.printf("[DBTEST] result: %s errors=%d\n", runtimeState.dbTestErrors == 0 ? "PASS" : "FAIL", runtimeState.dbTestErrors);
      sendEvent(EVT_MONITOR, runtimeState.dbTestErrors, runtimeState.dbTrackCount, runtimeState.dbTestErrors == 0 ? "DBTEST PASS" : "DBTEST FAIL");
    }

    if (runtimeState.dbFindRequest) {
      runtimeState.dbFindRequest = false;
      String q = String(runtimeState.dbFindQuery);
      q.trim();

      // v9.2-alpha7: indexed search still performs a long sequential SD read.
      // FLAC playback has exclusive priority over SD; reject this foreground
      // request instead of risking decoder starvation/card command failure.
      const bool flacOwnsSd = (runtimeState.mediaCodec == MEDIA_CODEC_FLAC) &&
                              (runtimeState.audioPlaying || runtimeState.rtAudioTaskRunning ||
                               runtimeState.mediaState == MEDIA_STATE_PLAYING);
      if (flacOwnsSd) {
        runtimeState.dbFindResults = 0;
        runtimeState.dbFindLastMs = 0;
        Serial.printf("[FIND] blocked during FLAC playback query='%s'; stop playback and retry\n", q.c_str());
        sendEvent(EVT_MONITOR, 0, runtimeState.dbTrackCount, "Search blocked: FLAC owns SD");
      } else {
        if (!storageService.ok) storageService.mount();
        databaseService.startIfMounted();
        uint32_t findT0 = millis();
        // v8.3 Media Library Pro: Search reads /iq200/db/library/search.idx instead
        // of scanning media_###.db volumes on every UI request.
        int found = libraryManager.searchIndex(q, runtimeState.dbFindLimit);
        runtimeState.dbFindResults = found;
        runtimeState.dbFindLastMs = millis() - findT0;
        Serial.printf("[FIND] query='%s' results=%d time=%lu ms mode=library-index\n", q.c_str(), found, (unsigned long)runtimeState.dbFindLastMs);
        sendEvent(EVT_MONITOR, found, runtimeState.dbTrackCount, "Library indexed find");
      }
    }

    // v8.0.3 Incremental Update Engine foundation.
    // update/upd does a fast DB validation and playlist reload. If the DB is missing
    // or invalid, it schedules a normal single-pass scan. This keeps boot fast and
    // avoids unnecessary full SD scans when media.meta + media.idx are valid.
    if (runtimeState.dbUpdateRequest) {
      runtimeState.dbUpdateRequest = false;
      runtimeState.dbUpdateBusy = true;
      uint32_t t0 = millis();
      if (!storageService.ok) storageService.mount();
      databaseService.startIfMounted();
      Serial.println("[DBUPD] incremental update check begin");
      bool valid = mediaDb.validateManifestFast(true);
      if (valid) {
        int loaded = mediaDb.loadPlaylist(playlist);
        if (playlist.size() > 0) setMediaPathFromPlaylist(playlist.currentTrack());
        runtimeState.playlistCount = playlist.size();
        runtimeState.playlistIndex = playlist.index();
        runtimeState.dbTrackCount = mediaDb.tracks();
        runtimeState.dbVolumeCount = mediaDb.volumes();
        runtimeState.dbArtCount = mediaDb.art();
        runtimeState.dbUpdateLoaded = loaded;
        runtimeState.dbUpdateLastMs = millis() - t0;
        Serial.printf("[DBUPD] OK no full scan needed loaded=%d/%d time=%lu ms\n",
                      loaded, runtimeState.dbTrackCount, (unsigned long)runtimeState.dbUpdateLastMs);
        sendEvent(EVT_MONITOR, loaded, runtimeState.dbTrackCount, "DB update OK");
      } else {
        runtimeState.dbUpdateLoaded = 0;
        runtimeState.dbUpdateLastMs = millis() - t0;
        Serial.println("[DBUPD] database invalid or missing; scheduling full scan");
        runtimeState.dbScanRequest = true;
        sendEvent(EVT_MONITOR, 0, 0, "DB update -> scan");
      }
      runtimeState.dbUpdateBusy = false;
    }

    if (runtimeState.dbLoadRequest) {
      runtimeState.dbLoadRequest = false;
      if (!storageService.ok) storageService.mount();
      databaseService.startIfMounted();
      int loaded = mediaDb.loadPlaylist(playlist);
      if (playlist.size() > 0) setMediaPathFromPlaylist(playlist.currentTrack());
      runtimeState.playlistCount = playlist.size();
      runtimeState.playlistIndex = playlist.index();
      Serial.printf("[DB] loaded active playlist: %d/%d from SD database\n", loaded, runtimeState.dbTrackCount);
      sendEvent(EVT_MONITOR, loaded, runtimeState.dbTrackCount, "DB loaded");
    }

    // v9.2-alpha2: a full database rebuild monopolizes SD bandwidth. Never
    // stop or starve the active decoder. Accept the command during playback,
    // but defer the actual scan until the player is completely idle.
    if (runtimeState.dbScanRequest) {
      runtimeState.dbScanRequest = false;
      const bool playbackActive = iqPlayerCore.isPlaying() || iqPlayerCore.isTaskRunning() ||
                                  runtimeState.audioPlaying || runtimeState.rtAudioTaskRunning ||
                                  runtimeState.audioBusy || runtimeState.trackHandoffActive;
      if (playbackActive) {
        runtimeState.dbScanDeferred = true;
        strncpy(runtimeState.scanMessage, "Scan deferred: stop playback", sizeof(runtimeState.scanMessage) - 1);
        runtimeState.scanMessage[sizeof(runtimeState.scanMessage) - 1] = 0;
        strncpy(runtimeState.lastMessage, "Scan queued until playback stops", sizeof(runtimeState.lastMessage) - 1);
        runtimeState.lastMessage[sizeof(runtimeState.lastMessage) - 1] = 0;
        runtimeState.uiDirty = true;
        Serial.println("[SCANSVC] deferred: playback active; use stop to start queued scan");
        sendEvent(EVT_MONITOR, 0, 0, "Scan deferred until stop");
      } else {
        scanService.requestFullScan("SD scan: background...");
      }
    }

    if (runtimeState.dbScanDeferred && !scanService.busy()) {
      const bool playbackActive = iqPlayerCore.isPlaying() || iqPlayerCore.isTaskRunning() ||
                                  runtimeState.audioPlaying || runtimeState.rtAudioTaskRunning ||
                                  runtimeState.audioBusy || runtimeState.trackHandoffActive;
      if (!playbackActive) {
        runtimeState.dbScanDeferred = false;
        Serial.println("[SCANSVC] playback idle: starting deferred SD scan");
        scanService.requestFullScan("Deferred SD scan: playback stopped");
      }
    }


    if (runtimeState.webTrackPlayRequest) {
      runtimeState.webTrackPlayRequest = false;
      const int target = runtimeState.webTrackPlayIndex;
      runtimeState.webTrackPlayIndex = -1;
      if (target < 0 || target >= playlist.size()) {
        Serial.printf("[WEBLIB] play rejected index=%d size=%d\n", target, playlist.size());
      } else {
        runtimeState.navPendingDelta = 0;
        runtimeState.navCommitPending = false;
        runtimeState.navPreviewActive = false;
        runtimeState.playlistNextRequest = false;
        runtimeState.playlistNextAutoPlayRequest = false;
        runtimeState.playlistPrevRequest = false;
        const bool active = iqPlayerCore.isPlaying() || iqPlayerCore.isTaskRunning() ||
                            runtimeState.audioPlaying || runtimeState.rtAudioTaskRunning;
        if (active) {
          suppressRtCompletionOnce = true;
          iqPlayerCore.stopAndWait(1500);
          vTaskDelay(pdMS_TO_TICKS(80));
        }
        if (playlist.select(target)) {
          setMediaPathFromPlaylist(playlist.currentTrack());
          runtimeState.playlistIndex = playlist.index();
          runtimeState.playlistCount = playlist.size();
          runtimeState.mediaProgress = 0;
          runtimeState.mediaPlayedBytes = 0;
          runtimeState.trackHandoffActive = true;
          runtimeState.trackHandoffAutoPlay = true;
          setHandoffState("WEB_LIBRARY");
          runtimeState.mediaState = MEDIA_STATE_LOADING;
          runtimeState.wavPlayRequest = true;
          runtimeState.uiDirty = true;
          Serial.printf("[WEBLIB] play index=%d track=%s\n", target, runtimeState.mediaPath);
          sendEvent(EVT_MONITOR, target, playlist.size(), "Web library play");
        }
      }
    }

    if (runtimeState.playlistScanRequest) {
      runtimeState.playlistScanRequest = false;
      if (!storageService.ok) storageService.mount();

      // v7.3.5: legacy file index remains WDT-safe; media database is used for full library.
      int indexed = fileIndex.scanRoot();
      runtimeState.fileIndexCount = indexed;

      playlist.clear();
      int added = 0;
      int addedMp3 = 0;
      int addedFlac = 0;
      int addedWav = 0;

      // v9.0 priority order: FLAC -> OPUS -> MP3 -> OGG -> WAV -> AAC.
      for (int pr = 0; pr <= 5; pr++) {
        for (int i = 0; i < fileIndex.size(); i++) {
          String path = normalizeRootPath(fileIndex.get(i));
          if (!isSupportedMediaPath(path)) continue;
          if (mediaPriority(path) != pr) continue;
          if (playlist.add(path)) {
            added++;
            if (pr == 0) addedMp3++;
            else if (pr == 1) addedFlac++;
            else if (pr == 2) addedWav++;
          }
        }
      }

      if (playlist.size() == 0) {
        strncpy(runtimeState.mediaPath, "", sizeof(runtimeState.mediaPath));
        strncpy(runtimeState.playlistCurrent, "", sizeof(runtimeState.playlistCurrent));
        strncpy(runtimeState.mediaTitle, "No media", sizeof(runtimeState.mediaTitle) - 1);
        runtimeState.mediaCodec = MEDIA_CODEC_NONE;
        runtimeState.mediaState = MEDIA_STATE_STOPPED;
        runtimeState.playlistCount = 0;
        runtimeState.playlistIndex = 0;
        Serial.printf("[CORE0] Playlist scan: indexed=%d added=0 mp3=0 flac=0 wav=0\n", indexed);
        Serial.println("[PL] no supported media files found on SD");
      } else {
        setMediaPathFromPlaylist(playlist.currentTrack());
        runtimeState.playlistCount = playlist.size();
        runtimeState.playlistIndex = playlist.index();
        Serial.printf("[CORE0] Playlist scan WDT-safe: indexed=%d added=%d mp3=%d flac=%d wav=%d count=%d current=%d/%d %s\n",
          indexed, added, addedMp3, addedFlac, addedWav, playlist.size(), playlist.index() + 1, playlist.size(), runtimeState.mediaPath);
        Serial.println("[PL] priority: FLAC -> OPUS -> MP3 -> OGG -> WAV -> AAC. Type list/pl, next/prev, play/p");
      }

      sendEvent(EVT_MONITOR, playlist.size(), indexed, "Playlist scan");
    }


    if (runtimeState.playlistListRequest) {
      runtimeState.playlistListRequest = false;
      runtimeState.playlistCount = playlist.size();
      runtimeState.playlistIndex = playlist.index();
      Serial.printf("[PL] Playlist (%d) current=%d\n", playlist.size(), playlist.index() + 1);
      if (playlist.size() == 0) {
        Serial.println("[PL] empty. Use scan/plscan first.");
      } else {
        int maxPrint = playlist.size();
        if (maxPrint > 32) maxPrint = 32;
        for (int i = 0; i < maxPrint; i++) {
          MediaCodec cdc = mediaCodecFromPath(playlist.at(i).c_str());
          Serial.printf("%c %02d  [%s] %s\n", i == playlist.index() ? '>' : ' ', i + 1, codecNameFromCodec(cdc), playlist.at(i).c_str());
        }
      }
      sendEvent(EVT_MONITOR, playlist.size(), playlist.index(), "Playlist list");
    }

    // v9.5-alpha3 Navigation Engine 2.0.
    // Rapid manual NEXT/PREV commands are coalesced into one in-memory preview.
    // Decoder/artwork/SD open are deferred until the encoder/command stream is idle.
    if (runtimeState.navPreviewEnabled && runtimeState.navCommitPending) {
      if (playlist.size() <= 0) {
        runtimeState.navPendingDelta = 0;
        runtimeState.navCommitPending = false;
        runtimeState.navPreviewActive = false;
        Serial.println("[NAV2] ignored: empty playlist");
      }
      int32_t delta = playlist.size() > 0 ? runtimeState.navPendingDelta : 0;
      if (delta != 0) {
        runtimeState.navPendingDelta = 0;

        if (!runtimeState.navPreviewActive) {
          runtimeState.navPreviewActive = true;
          runtimeState.navManualControlPending = true;
          // A manual preview supersedes any EOF-generated NEXT still in flight.
          runtimeState.playlistNextRequest = false;
          runtimeState.playlistNextAutoPlayRequest = false;
          runtimeState.playlistPrevRequest = false;
          runtimeState.navAutoPlayAfterCommit = iqPlayerCore.isPlaying() || iqPlayerCore.isTaskRunning() ||
                                                  runtimeState.audioPlaying || runtimeState.rtAudioTaskRunning;
          if (runtimeState.navAutoPlayAfterCommit) {
            Serial.println("[NAV2] preview begin: stopping decoder once");
            suppressRtCompletionOnce = true;
            iqPlayerCore.stopAndWait(1500);
            vTaskDelay(pdMS_TO_TICKS(80));
          }
        }

        const bool forward = delta > 0;
        uint32_t moves = (uint32_t)(delta > 0 ? delta : -delta);
        if (moves > (uint32_t)playlist.size()) moves %= (uint32_t)playlist.size();
        if (moves == 0 && playlist.size() > 0) moves = playlist.size();
        for (uint32_t i = 0; i < moves; ++i) {
          if (forward) playlist.next(); else playlist.prev();
        }
        setMediaPathFromPlaylist(playlist.currentTrack());
        runtimeState.mediaProgress = 0;
        runtimeState.mediaPlayedBytes = 0;
        runtimeState.wavProgress = 0;
        runtimeState.wavPlayedBytes = 0;
        runtimeState.mediaVuLeft = 0;
        runtimeState.mediaVuRight = 0;
        runtimeState.mediaState = MEDIA_STATE_READY;
        strncpy(runtimeState.playerStateName, "PREVIEW", sizeof(runtimeState.playerStateName) - 1);
        runtimeState.playerStateName[sizeof(runtimeState.playerStateName) - 1] = 0;
        runtimeState.navPreviewMoves += moves;
        runtimeState.uiDirty = true;
        Serial.printf("[NAV2] preview delta=%ld selected=%s (%d/%d) autoplay=%d\n",
          (long)delta, runtimeState.mediaPath, playlist.index() + 1, playlist.size(),
          runtimeState.navAutoPlayAfterCommit ? 1 : 0);
        sendEvent(EVT_MONITOR, playlist.index(), playlist.size(), "Navigation preview");
      }

      const uint32_t idleMs = (uint32_t)(millis() - runtimeState.navLastInputMs);
      if (runtimeState.navPreviewActive && runtimeState.navPendingDelta == 0 &&
          idleMs >= runtimeState.navCommitDelayMs) {
        runtimeState.navCommitPending = false;
        runtimeState.navPreviewActive = false;
        runtimeState.navCommits++;
        Serial.printf("[NAV2] commit selected=%s (%d/%d) idle=%lums autoplay=%d\n",
          runtimeState.mediaPath, playlist.index() + 1, playlist.size(),
          (unsigned long)idleMs, runtimeState.navAutoPlayAfterCommit ? 1 : 0);
        if (runtimeState.navAutoPlayAfterCommit) {
          runtimeState.trackHandoffActive = true;
          runtimeState.trackHandoffAutoPlay = true;
          setHandoffState("NAV_COMMIT");
          runtimeState.mediaState = MEDIA_STATE_LOADING;
          runtimeState.wavPlayRequest = true;
        } else {
          strncpy(runtimeState.playerStateName, "READY", sizeof(runtimeState.playerStateName) - 1);
          runtimeState.playerStateName[sizeof(runtimeState.playerStateName) - 1] = 0;
          setHandoffState("SELECTED");
        }
        runtimeState.navAutoPlayAfterCommit = false;
        runtimeState.uiDirty = true;
        sendEvent(EVT_MONITOR, playlist.index(), playlist.size(), "Navigation commit");
      }
    }

    if (runtimeState.playlistNextRequest) {
      runtimeState.playlistNextRequest = false;
      const bool forceAutoPlay = runtimeState.playlistNextAutoPlayRequest;
      runtimeState.playlistNextAutoPlayRequest = false;
      const uint32_t handoffAge = (uint32_t)(millis() - runtimeState.trackHandoffLastMs);
      if (!forceAutoPlay && runtimeState.trackHandoffLastMs != 0 && handoffAge < 600) {
        Serial.printf("[HANDOFF] NEXT ignored: SD cooldown %lu/600 ms\n", (unsigned long)handoffAge);
      } else if (!atomicPlaylistSwitch(true, suppressRtCompletionOnce, forceAutoPlay)) {
        if (playlist.size() == 0) Serial.println("[PL] next ignored: empty playlist");
      } else {
        Serial.printf("[HANDOFF] NEXT selected: %s (%d/%d) autoplay=%d source=%s\n",
          runtimeState.mediaPath, playlist.index() + 1, playlist.size(),
          runtimeState.trackHandoffAutoPlay ? 1 : 0,
          forceAutoPlay ? "EOF" : "USER");
        sendEvent(EVT_MONITOR, playlist.index(), playlist.size(), forceAutoPlay ? "Auto next track" : "Next track handoff");
      }
    }

    if (runtimeState.playlistPrevRequest) {
      runtimeState.playlistPrevRequest = false;
      const uint32_t handoffAge = (uint32_t)(millis() - runtimeState.trackHandoffLastMs);
      if (runtimeState.trackHandoffLastMs != 0 && handoffAge < 600) {
        Serial.printf("[HANDOFF] PREV ignored: SD cooldown %lu/600 ms\n", (unsigned long)handoffAge);
      } else if (!atomicPlaylistSwitch(false, suppressRtCompletionOnce)) {
        if (playlist.size() == 0) Serial.println("[PL] prev ignored: empty playlist");
      } else {
        Serial.printf("[HANDOFF] PREV selected: %s (%d/%d) autoplay=%d\n",
          runtimeState.mediaPath, playlist.index() + 1, playlist.size(),
          runtimeState.trackHandoffAutoPlay ? 1 : 0);
        sendEvent(EVT_MONITOR, playlist.index(), playlist.size(), "Prev track handoff");
      }
    }

    if (runtimeState.wavOpenRequest && !runtimeState.audioBusy) {
      runtimeState.wavOpenRequest = false;
      runtimeState.audioBusy = true;
      Serial.println("[CORE0] WAV open begin");
      if (!storageService.ok) storageService.mount();

      bool ok = audioStream.openWav("/test.wav");
      runtimeState.wavValid = ok;
      runtimeState.audioPlaying = ok;

      if (ok) {
        runtimeState.wavSampleRate = audioStream.info().sampleRate;
        runtimeState.wavChannels = audioStream.info().channels;
        runtimeState.wavDataSize = audioStream.info().dataSize;
        sendEvent(EVT_WAV_OPENED, 1, audioStream.info().dataSize, "WAV opened");
      } else {
        sendEvent(EVT_WAV_OPENED, 0, 0, "WAV fail");
      }

      runtimeState.audioBusy = false;
      Serial.printf("[CORE0] WAV open done: %d\n", ok);
    }



    if (runtimeState.playWavFallbackRequest) {
      runtimeState.playWavFallbackRequest = false;
      runtimeState.playWavTestMode = true;
      if (selectNextTrackByCodec(MEDIA_CODEC_WAV)) {
        Serial.printf("[IQPLAYER] playwav selected: %s (%d/%d)\n", runtimeState.mediaPath, playlist.index() + 1, playlist.size());
        runtimeState.wavPlayRequest = true;
      } else {
        Serial.println("[IQPLAYER] playwav failed: no WAV in current DB/playlist");
        sendEvent(EVT_MONITOR, 0, 0, "no WAV track");
      }
    }

    // Local playback always wins a new explicit request. Ask WebRadio to
    // release I2S asynchronously and keep wavPlayRequest armed until it does.
    if (runtimeState.wavPlayRequest && radioService.isActive()) {
      static uint32_t lastRadioReleaseLogMs = 0;
      radioService.stop();
      if (millis() - lastRadioReleaseLogMs > 1000) {
        lastRadioReleaseLogMs = millis();
        Serial.println("[AUDIO][OWNER] local play waiting for WebRadio I2S release");
      }
    }

    if (runtimeState.wavPlayRequest && !radioService.isActive() &&
        (runtimeState.audioBusy || wavPlayer.isPlaying() || wavPlayer.isTaskRunning())) {
      runtimeState.wavPlayRequest = false;
      Serial.println("[CORE0] play ignored: IQPlayerCore already active/busy");
      sendEvent(EVT_MONITOR, 0, runtimeState.wavPlayedBytes, "WAV already active");
    }

    if (runtimeState.wavPlayRequest && !radioService.isActive() && !runtimeState.audioBusy) {
      runtimeState.wavPlayRequest = false;
      runtimeState.audioBusy = true;

      const char* playPath = runtimeState.mediaPath[0] ? runtimeState.mediaPath : "/test.wav";
      MediaCodec playCodec = mediaCodecFromPath(playPath);
      runtimeState.mediaCodec = playCodec;
      if (playCodec != MEDIA_CODEC_WAV) runtimeState.playWavTestMode = false;

      // Artwork must be searched only after SD is confirmed mounted. Previously a
      // failed early lookup could cache CD_DISC/ERROR for the album indefinitely.
      if (!storageService.ok) {
        Serial.println("[IQPLAYER] SD not mounted, mounting...");
        bool sdok = storageService.mount();
        Serial.printf("[IQPLAYER] SD mount result: %d MB=%llu\n", sdok, storageService.mb);
      }

      // Load once per album directory. Supports JPEG and PNG and retries transient failures.
      ArtworkCache::instance().prepareForTrack(playPath);

      // v9.8-alpha41: give Core1 a bounded pre-play window to decode and draw
      // the newly cached artwork before the RT audio task starts. During playback
      // TFT decoding remains forbidden; Web can still serve the compressed cache.
      const uint32_t artGen = ArtworkCache::instance().generation();
      runtimeState.artworkRenderRequestGeneration = artGen;
      const uint32_t artWaitStart = millis();
      while (runtimeState.artworkRenderedGeneration != artGen &&
             (uint32_t)(millis() - artWaitStart) < 900U) {
        vTaskDelay(pdMS_TO_TICKS(10));
      }
      Serial.printf("[ART][PREPLAY] gen=%lu rendered=%lu wait=%lums\n",
                    (unsigned long)artGen,
                    (unsigned long)runtimeState.artworkRenderedGeneration,
                    (unsigned long)(millis() - artWaitStart));

      // v9.8-alpha31: persist the selected current track before opening the
      // long-lived audio stream. During playback SDManager::streamActive()
      // intentionally blocks resume writes, which previously meant the new
      // track was never committed to RESUME.
      runtimeState.volumePercent = audioEngine.getVolume();
      runtimeState.playlistCount = playlist.size();
      runtimeState.playlistIndex = playlist.index();
      runtimeState.eqEnabled = audioEngine.equalizerEnabled();
      runtimeState.eqBassDb = audioEngine.equalizerBass();
      runtimeState.eqMidDb = audioEngine.equalizerMid();
      runtimeState.eqTrebleDb = audioEngine.equalizerTreble();
      strncpy(runtimeState.eqPreset, audioEngine.equalizerPreset(), sizeof(runtimeState.eqPreset)-1);
      runtimeState.eqPreset[sizeof(runtimeState.eqPreset)-1] = 0;
      const bool resumeTrackSaved = resumeEngine.save();
      Serial.printf("[SMARTRESUME] current track commit ok=%d path=%s index=%d/%d\n",
                    resumeTrackSaved ? 1 : 0, runtimeState.mediaPath,
                    runtimeState.playlistIndex + 1, runtimeState.playlistCount);

      Serial.printf("[IQPLAYER] play request [%s] %s\n", codecNameFromCodec(playCodec), playPath);
      bool ok = iqPlayerCore.play(playPath);
      const bool completedHandoff = runtimeState.trackHandoffActive;
      runtimeState.trackHandoffActive = false;
      runtimeState.trackHandoffAutoPlay = false;
      setHandoffState(ok ? "PLAYING" : "START_FAILED");
      if (completedHandoff) {
        Serial.printf("[HANDOFF] start %s codec=%s path=%s\n",
          ok ? "OK" : "FAILED", codecNameFromCodec(playCodec), playPath);
      }
      if (ok) {
        runtimeState.navManualControlPending = false;
        runtimeState.radioTakeoverStop = false;
        sendEvent(EVT_WAV_STARTED, 1, runtimeState.mediaDataSize, "IQPlayer play");
      } else {
        // v9.1.2: unsupported decoder / open failure must not leave UI in LOADING
        // and must not keep the command path busy.
        runtimeState.audioBusy = false;
        runtimeState.audioPlaying = false;
        runtimeState.rtAudioTaskRunning = false;
        if (runtimeState.mediaCodec != MEDIA_CODEC_WAV) runtimeState.mediaState = MEDIA_STATE_READY;
        sendEvent(EVT_MONITOR, 0, 0, iqPlayerCore.lastError());
      }
      runtimeState.audioBusy = false;
    }

    // v7.1 RT Audio: WAV is written to I2S by a dedicated high-priority Core0 task.
    // core0Worker only mirrors lightweight status for UI/broker.
    {
      static bool lastWavPlaying = false;
      iqPlayerCore.mirror();
      bool nowPlaying = iqPlayerCore.isPlaying();
      mediaEngine.mirrorFromRuntime();

      static uint32_t lastWavEvent = 0;
      if (nowPlaying && millis() - lastWavEvent > 250) {
        lastWavEvent = millis();
        sendEvent(EVT_WAV_PROGRESS, runtimeState.mediaProgress, runtimeState.mediaPlayedBytes, "RT progress");
      }

      if (lastWavPlaying && !nowPlaying) {
        runtimeState.audioPlaying = false;
        runtimeState.wavVuLeft = 0;
        runtimeState.wavVuRight = 0;
        const char* doneCodec = codecNameFromCodec((MediaCodec)runtimeState.mediaCodec);
        // v9.2-alpha20: a genuine decoder EOF has priority over a stale
        // one-shot suppression flag. The old logic checked suppression first,
        // so an earlier manual handoff could make the next natural EOF look like
        // a control STOP and prevent Auto Next from being queued.
        const bool naturalEof =
          (strcmp(runtimeState.playerStateName, "EOF") == 0) ||
          (runtimeState.mediaProgress >= 100 && runtimeState.mediaPlayedBytes >= runtimeState.mediaDataSize && runtimeState.mediaDataSize > 0);

        const bool manualNavigationStop =
          runtimeState.navManualControlPending || runtimeState.navPreviewActive ||
          runtimeState.navCommitPending || runtimeState.radioTakeoverStop;

        if ((suppressRtCompletionOnce || runtimeState.radioTakeoverStop) &&
            (manualNavigationStop || !naturalEof)) {
          suppressRtCompletionOnce = false;
          runtimeState.navManualControlPending = false;
          runtimeState.radioTakeoverStop = false;
          // Intentional STOP/handoff, not decoder EOF. Keep current progress and
          // never convert the selected path to STOPPED 100%.
          if (runtimeState.mediaState != MEDIA_STATE_ERROR) {
            runtimeState.mediaState = MEDIA_STATE_STOPPED;
          }
          Serial.printf("[CORE0] %s RT stopped by control/handoff at %u%% underruns=%lu shortWrites=%lu health=%u\n",
            doneCodec,
            (unsigned)runtimeState.mediaProgress,
            (unsigned long)runtimeState.audioUnderruns,
            (unsigned long)runtimeState.audioShortWrites,
            (unsigned int)runtimeState.audioHealth);
          sendEvent(EVT_WAV_STOPPED, 0, runtimeState.mediaPlayedBytes, "RT stopped");
        } else if (runtimeState.mediaState == MEDIA_STATE_ERROR) {
          Serial.printf("[CORE0][SDERR] %s RT aborted at %u%% played=%lu health=%u msg=%s\n",
            doneCodec,
            (unsigned)runtimeState.mediaProgress,
            (unsigned long)runtimeState.mediaPlayedBytes,
            (unsigned int)runtimeState.audioHealth,
            runtimeState.lastMessage);
          sendEvent(EVT_WAV_STOPPED, 0, runtimeState.mediaPlayedBytes, "RT SD error");
        } else {
          if (suppressRtCompletionOnce) {
            Serial.println("[AUTONEXT][FSM] natural EOF overrides stale suppress; clearing");
            suppressRtCompletionOnce = false;
          }
          // v9.1.2: only true decoder EOF may become 100%. SD/read failure is
          // handled above as ERROR and keeps the last good progress.
          runtimeState.wavProgress = 100;
          runtimeState.wavPlayedBytes = runtimeState.wavDataSize;
          runtimeState.mediaState = MEDIA_STATE_STOPPED;
          runtimeState.mediaProgress = 100;
          runtimeState.mediaPlayedBytes = runtimeState.mediaDataSize ? runtimeState.mediaDataSize : runtimeState.wavDataSize;
          Serial.printf("[CORE0] %s RT finished 100%% underruns=%lu shortWrites=%lu health=%u\n",
            doneCodec,
            (unsigned long)runtimeState.audioUnderruns,
            (unsigned long)runtimeState.audioShortWrites,
            (unsigned int)runtimeState.audioHealth);
          sendEvent(EVT_WAV_STOPPED, 1, runtimeState.mediaPlayedBytes, "RT finished");

          // v9.2-alpha14: never start a new decoder from inside the EOF mirror
          // transition. Queue the proven NEXT handoff for the next Core0 loop,
          // after IQPlayerCore and RuntimeState have fully settled to STOPPED.
          if (playlist.size() > 0 &&
              !runtimeState.navManualControlPending &&
              !runtimeState.navPreviewActive &&
              !runtimeState.navCommitPending &&
              strcmp(runtimeState.trackHandoffState, "NAV_COMMIT") != 0) {
            // A genuine EOF always starts a fresh Auto Next cycle. Clear any
            // stale manual-handoff suppression left by an earlier transition.
            if (suppressRtCompletionOnce) {
              Serial.println("[AUTONEXT][FSM] stale suppress detected at EOF; clearing");
              suppressRtCompletionOnce = false;
            }
            runtimeState.playlistNextAutoPlayRequest = true;
            runtimeState.playlistNextRequest = true;
            Serial.printf("[AUTONEXT] EOF queued current=%d/%d\n",
                          playlist.index() + 1, playlist.size());
            sendEvent(EVT_MONITOR, playlist.index(), playlist.size(), "Auto next queued");
          } else if (playlist.size() <= 0) {
            Serial.println("[AUTONEXT] ignored: empty playlist");
          } else {
            Serial.println("[AUTONEXT][FSM] suppressed: manual navigation owns transition");
            runtimeState.playlistNextRequest = false;
            runtimeState.playlistNextAutoPlayRequest = false;
          }
        }
      }
      lastWavPlaying = nowPlaying;
    }

    if (runtimeState.wavStopRequest) {
      runtimeState.wavStopRequest = false;
      Serial.println("[CORE0] IQPlayer stop request");
      suppressRtCompletionOnce = true;
      iqPlayerCore.stop();
      runtimeState.audioPlaying = false;
      runtimeState.mediaState = MEDIA_STATE_STOPPED;
      runtimeState.mediaProgress = 0;
      sendEvent(EVT_WAV_STOPPED, 1, runtimeState.wavPlayedBytes, "WAV stopped");
    }

    if (runtimeState.audioToneRequest && !runtimeState.audioBusy) {
      runtimeState.audioToneRequest = false;
      runtimeState.audioBusy = true;
      Serial.println("[CORE0] tone begin");
      audioEngine.toneTest(250);
      sendEvent(EVT_AUDIO_DONE, 1, 0, "Tone done");
      runtimeState.audioBusy = false;
      Serial.println("[CORE0] tone done");
    }

    processLogCore0();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void core1UiTask(void* parameter) {
  Serial.printf("[TASK] core1UiTask running on core %d\n", xPortGetCoreID());

  ui.begin();

  for (;;) {
    runtimeState.core1Loops++;
    watchdogService.beatCore1();
    runtimeState.core0AgeMs = watchdogService.core0AgeMs();
    runtimeState.core1AgeMs = watchdogService.core1AgeMs();
    runtimeState.core0Ok = watchdogService.core0Ok();
    runtimeState.core1Ok = watchdogService.core1Ok();
    // v9.3-alpha2.4: deterministic UI scheduler. Avoid calling every service
    // at the ~2 kHz Core1 loop rate. Input stays responsive while animation,
    // notifications and generic services run at bounded rates.
    const uint32_t uiNow = millis();
    static uint32_t lastGuiMs = 0, lastInputMs = 0, lastAnimMs = 0;
    static uint32_t lastNotifyMs = 0, lastServiceMs = 0;
    if ((uint32_t)(uiNow - lastInputMs) >= 2) { lastInputMs = uiNow; schedulerService.tickInput(); }
    if ((uint32_t)(uiNow - lastGuiMs) >= 5) { lastGuiMs = uiNow; schedulerService.tickGUI(); }
    if ((uint32_t)(uiNow - lastAnimMs) >= 16) { lastAnimMs = uiNow; schedulerService.tickAnimation(); }
    if ((uint32_t)(uiNow - lastNotifyMs) >= 50) { lastNotifyMs = uiNow; schedulerService.tickNotification(); }
    if ((uint32_t)(uiNow - lastServiceMs) >= 10) { lastServiceMs = uiNow; serviceManager.tick(); }
    runtimeState.guiTicks = schedulerService.stats.guiTicks;
    runtimeState.inputTicks = schedulerService.stats.inputTicks;
    runtimeState.animationTicks = schedulerService.stats.animationTicks;


    IQEvent ev;
    uint8_t eventsThisPass = 0;
    while (iqEventQueue && eventsThisPass < 6 && xQueueReceive(iqEventQueue, &ev, 0) == pdTRUE) {
      eventsThisPass++;
      messageBroker.eventConsumed(ev.type);
      if (processLogEnabled && ev.type != EVT_WAV_PROGRESS) {
        Serial.printf("[PROC][EVT-] core=%d task=%s type=%s value=%d value64=%llu msg=%s\n",
                      xPortGetCoreID(), pcTaskGetName(nullptr), processEventName(ev.type), ev.value,
                      (unsigned long long)ev.value64, ev.message);
      }
      strncpy(runtimeState.lastMessage, ev.message, sizeof(runtimeState.lastMessage) - 1);
      runtimeState.lastMessage[sizeof(runtimeState.lastMessage) - 1] = 0;
      runtimeState.lastCore0Event++;
      runtimeState.uiDirty = true;

      if (ev.type == EVT_WIFI_DONE) {
        runtimeState.wifiNetworks = ev.value;
      } else if (ev.type == EVT_SD_DONE) {
        runtimeState.sdOk = ev.value != 0;
        runtimeState.sdMB = ev.value64;
      } else if (ev.type == EVT_INDEX_DONE) {
        runtimeState.fileIndexCount = ev.value;
      } else if (ev.type == EVT_AUDIO_STREAM_DONE) {
        runtimeState.audioPlaying = false;
      } else if (ev.type == EVT_WAV_OPENED) {
        runtimeState.wavValid = ev.value != 0;
        runtimeState.audioPlaying = ev.value != 0;
      } else if (ev.type == EVT_WAV_STARTED) {
        runtimeState.wavValid = ev.value != 0;
        runtimeState.audioPlaying = ev.value != 0;
      } else if (ev.type == EVT_WAV_PROGRESS) {
        runtimeState.wavProgress = ev.value;
        runtimeState.wavPlayedBytes = ev.value64;
      } else if (ev.type == EVT_WAV_STOPPED) {
        runtimeState.audioPlaying = false;
      } else if (ev.type == EVT_MONITOR) {
        // reserved for future monitor events
      }
    }

    const bool modeGestureActive = requestCenterOnDualHold();
    if (!modeGestureActive) ui.tick();

    String c;
    bool commandReady = false;
    bool commandFromWeb = false;
    if (Serial.available()) {
      c = Serial.readStringUntil('\n');
      commandReady = true;
    } else if (webServerService.popCommand(c)) {
      commandReady = true;
      commandFromWeb = true;
    }
    if (commandReady) {
      c.trim();
      c.toLowerCase();
      if (commandFromWeb) Serial.printf("[WEB][CMD] %s\n", c.c_str());
      blackBox.record(BlackBoxService::KIND_COMMAND, "CMD", 0, 0, c.c_str());
      if (processLogEnabled) {
        Serial.printf("[PROC][CMD] t=%lu core=%d task=%s cmd='%s' player=%s play=%d busy=%d scan=%d\n",
                      (unsigned long)millis(), xPortGetCoreID(), pcTaskGetName(nullptr), c.c_str(),
                      runtimeState.playerStateName, runtimeState.audioPlaying ? 1 : 0,
                      runtimeState.audioBusy ? 1 : 0, runtimeState.dbScanBusy ? 1 : 0);
      }

      if (handleModeCommand(c)) {
        // Clean reboot is executed by Core0 after this command response/log.
      } else if (commandManager.handleDiscovery(c, Serial)) {
        // handled by centralized command registry
      } else if (c == "bb on") {
        blackBox.setEnabled(true);
        blackBox.status(Serial);
      } else if (c == "bb off") {
        blackBox.setEnabled(false);
        blackBox.status(Serial);
      } else if (c == "bb" || c == "bb status") {
        blackBox.status(Serial);
      } else if (c == "bb clear") {
        blackBox.clear();
        blackBox.status(Serial);
      } else if (c == "bb dump") {
        blackBox.dump(Serial);
      } else if (c.startsWith("bb dump ")) {
        size_t limit = (size_t)c.substring(8).toInt();
        if (limit < 1) limit = 64;
        if (limit > blackBox.capacity()) limit = blackBox.capacity();
        blackBox.dump(Serial, limit);
      } else if (c == "ui" || c == "ui status") {
        ui.printProfiler(Serial);
      } else if (c == "ui reset") {
        ui.resetProfiler();
      } else if (c == "sd status" || c == "sd stats") {
        SDManager::printStats(Serial);
      } else if (c == "sd 16" || c == "sd speed 16" || c == "sd clock 16") {
        const bool ok = SDManager::setFrequency(16000000U);
        if (ok) { Preferences p; p.begin("iq200-sd", false); p.putUInt("clock", 16000000U); p.end(); }
        Serial.printf("[SD][CLOCK] 16 MHz %s%s\n", ok ? "OK" : "FAILED", SDManager::streamActive() ? " (stop playback first)" : "");
      } else if (c == "sd 12" || c == "sd speed 12" || c == "sd clock 12") {
        const bool ok = SDManager::setFrequency(12000000U);
        if (ok) { Preferences p; p.begin("iq200-sd", false); p.putUInt("clock", 12000000U); p.end(); }
        Serial.printf("[SD][CLOCK] 12 MHz %s%s\n", ok ? "OK" : "FAILED", SDManager::streamActive() ? " (stop playback first)" : "");
      } else if (c == "sd 10" || c == "sd speed 10" || c == "sd clock 10") {
        const bool ok = SDManager::setFrequency(10000000U);
        if (ok) { Preferences p; p.begin("iq200-sd", false); p.putUInt("clock", 10000000U); p.end(); }
        Serial.printf("[SD][CLOCK] 10 MHz %s%s\n", ok ? "OK" : "FAILED", SDManager::streamActive() ? " (stop playback first)" : "");
      } else if (c == "perf") {
        Serial.println("========== IQ200 PERFORMANCE ==========");
        Serial.printf("Player     : %s codec=%u progress=%u%% health=%u%%\n", runtimeState.playerStateName, (unsigned)runtimeState.mediaCodec, (unsigned)runtimeState.mediaProgress, (unsigned)runtimeState.audioHealth);
        Serial.printf("Audio      : play=%d rt=%d underrun=%lu short=%lu\n", runtimeState.audioPlaying ? 1 : 0, runtimeState.rtAudioTaskRunning ? 1 : 0, (unsigned long)runtimeState.audioUnderruns, (unsigned long)runtimeState.audioShortWrites);
        Serial.printf("Memory     : heap=%lu psram=%lu\n", (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getFreePsram());
        Serial.printf("Tasks      : C0stack=%lu C1stack=%lu\n", (unsigned long)runtimeState.core0StackHighWater, (unsigned long)runtimeState.core1StackHighWater);
        Serial.printf("Queue      : %u/12 posts=%lu drops=%lu coal=%lu\n", iqEventQueue ? (unsigned)uxQueueMessagesWaiting(iqEventQueue) : 0U, (unsigned long)runtimeState.eventBusPosts, (unsigned long)runtimeState.eventBusDrops, (unsigned long)messageBroker.coalesced());
        blackBox.status(Serial);
        SDManager::printStats(Serial);
        Serial.println("=======================================");
      } else if (c == "diagnostics off" || c == "diaglog off" || c == "log silent") {
        diagnosticsOutputSet(false, true);
      } else if (c == "diagnostics on" || c == "diaglog on") {
        diagnosticsOutputSet(true, true);
        processLogStatus();
      } else if (c == "diagnostics status" || c == "diaglog status") {
        if (!diagnosticsOutputEnabled) {
          // Normally visible only after enabling from Web; kept for command symmetry.
        } else {
          Serial.printf("[DIAG] output=%s process=%s interval=%lums\n",
                        diagnosticsOutputEnabled ? "ON" : "OFF",
                        processLogEnabled ? "ON" : "OFF",
                        (unsigned long)processLogIntervalMs);
        }
      } else if (c == "log on") {
        processLogEnabled = true;
        processLogStatus();
      } else if (c == "log off") {
        processLogEnabled = false;
        Serial.println("[PROC][CFG] disabled");
      } else if (c == "log" || c == "log status") {
        processLogStatus();
      } else if (c.startsWith("log rate ")) {
        const uint32_t requested = (uint32_t)c.substring(9).toInt();
        if (requested >= 250 && requested <= 10000) {
          processLogIntervalMs = requested;
          processLogStatus();
        } else {
          Serial.println("[PROC][CFG] usage: log rate 250..10000");
        }
      } else if (c == "log reset") {
        SDManager::resetStats();
        Serial.println("[PROC][CFG] counters reset");
      } else if (c == "eq" || c == "eq status" || c == "eq list") {
        audioEngine.printEqualizer(Serial);
      } else if (c.startsWith("eq custom ")) {
        int b=0,m=0,t=0;
        if (sscanf(c.c_str()+10, "%d %d %d", &b, &m, &t) == 3) {
          audioEngine.setEqualizer(true,b,m,t,"custom");
          runtimeState.eqEnabled=true; runtimeState.eqBassDb=audioEngine.equalizerBass();
          runtimeState.eqMidDb=audioEngine.equalizerMid(); runtimeState.eqTrebleDb=audioEngine.equalizerTreble();
          strncpy(runtimeState.eqPreset,"custom",sizeof(runtimeState.eqPreset)-1);
          runtimeState.eqPreset[sizeof(runtimeState.eqPreset)-1]=0;
          runtimeState.resumeAutoSaveRequest=true; smartResumeService.markDirty();
        } else Serial.println("[EQ] usage: eq custom <bass> <mid> <treble>");
      } else if (c.startsWith("eq ")) {
        String preset=c.substring(3); preset.trim();
        if (!audioEngine.setEqualizerPreset(preset)) audioEngine.printEqualizer(Serial);
        else {
          runtimeState.eqEnabled=audioEngine.equalizerEnabled();
          runtimeState.eqBassDb=audioEngine.equalizerBass(); runtimeState.eqMidDb=audioEngine.equalizerMid();
          runtimeState.eqTrebleDb=audioEngine.equalizerTreble();
          strncpy(runtimeState.eqPreset,audioEngine.equalizerPreset(),sizeof(runtimeState.eqPreset)-1);
          runtimeState.eqPreset[sizeof(runtimeState.eqPreset)-1]=0;
          runtimeState.resumeAutoSaveRequest=true; smartResumeService.markDirty();
        }
      } else if (c == "reboot") ESP.restart();
      else ui.tickSerialCommand(c);
    }

    processLogCore1();
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

void webServiceTask(void* parameter) {
  (void)parameter;
  runtimeState.webTaskRunning = true;
  Serial.printf("[TASK] webServiceTask running on core %d priority=%u\n",
                xPortGetCoreID(), (unsigned)uxTaskPriorityGet(nullptr));
  for (;;) {
    runtimeState.webTaskLoops++;
    if ((runtimeState.webTaskLoops & 0x3FU) == 0U) {
      runtimeState.webTaskStackHighWater = uxTaskGetStackHighWaterMark(nullptr);
    }
    webServerService.tick();
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

void modeCenterTask(void* parameter) {
  (void)parameter;
  Serial.printf("[TASK] modeCenterTask running on core %d\n", xPortGetCoreID());
  for (;;) {
    runtimeState.core1Loops++;
    modeCenterUi.tick();
    if (Serial.available()) {
      String command = Serial.readStringUntil('\n');
      command.trim();
      command.toLowerCase();
      handleModeCommand(command);
      processModeSwitchRequest();
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

void webRadioCoreTask(void* parameter) {
  (void)parameter;
  Serial.printf("[TASK] webRadioCoreTask running on core %d\n", xPortGetCoreID());
  for (;;) {
    runtimeState.core0Loops++;
    watchdogService.beatCore0();
    // The dedicated Web task is preferred, but keep WebRadio usable if task
    // allocation fails under an unusually fragmented heap.
    if (!webTaskCreated) webServerService.tick();
    runtimeState.core0HeapFree = ESP.getFreeHeap();
    runtimeState.core0StackHighWater = uxTaskGetStackHighWaterMark(nullptr);
    connectivityManager.tick();
    radioService.tick();
    stabilityService.tick();
    commercialPolishService.tick();
    processNetworkRequests();
    modeHealthTick();
    processModeSwitchRequest();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void webRadioUiTask(void* parameter) {
  (void)parameter;
  Serial.printf("[TASK] webRadioUiTask running on core %d\n", xPortGetCoreID());
  for (;;) {
    runtimeState.core1Loops++;
    watchdogService.beatCore1();
    runtimeState.core1HeapFree = ESP.getFreeHeap();
    runtimeState.core1StackHighWater = uxTaskGetStackHighWaterMark(nullptr);
    const bool modeGestureActive = requestCenterOnDualHold();
    if (!modeGestureActive) webRadioModeUi.tick();

    String command;
    bool ready = false;
    if (Serial.available()) {
      command = Serial.readStringUntil('\n');
      ready = true;
    } else if (webServerService.popCommand(command)) {
      ready = true;
    }
    if (ready) {
      command.trim();
      command.toLowerCase();
      if (handleModeCommand(command)) {
      } else if (commandManager.handleDiscovery(command, Serial)) {
      } else if (command == "stop" || command == "radio stop") {
        radioService.stop();
      } else if (command == "radio" || command == "radio status" || command == "status") {
        radioService.print();
        webServerService.print();
      } else if (command.startsWith("volume ") || command.startsWith("vol ")) {
        const int split = command.indexOf(' ');
        const int value = split >= 0 ? command.substring(split + 1).toInt() : -1;
        if (value >= 0 && value <= 100) {
          webRadioModeUi.setVolume(value);
          Serial.printf("[VOLUME] WebRadio=%d%%\n", value);
        } else Serial.println("[VOLUME] usage: volume 0..100");
      } else if (webRadioModeUi.handleCommand(command)) {
      } else if (command == "diagnostics off") {
        diagnosticsOutputSet(false, true);
      } else if (command == "diagnostics on") {
        diagnosticsOutputSet(true, true);
      } else if (command == "reboot") {
        ESP.restart();
      } else if (command.length()) {
        Serial.printf("[WEBRADIO][CMD] unsupported in this mode: %s\n", command.c_str());
      }
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

static bool startDedicatedWebTask() {
  const BaseType_t webTaskRc = xTaskCreatePinnedToCore(
    webServiceTask,
    "web_service",
    8192,
    nullptr,
    2,
    &webTaskHandle,
    0
  );
  webTaskCreated = webTaskRc == pdPASS;
  runtimeState.webTaskRunning = webTaskCreated;
  Serial.printf("[TASK] web_service create=%s stack=8192\n",
                webTaskCreated ? "OK" : "FAIL_FALLBACK");
  return webTaskCreated;
}

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(10);  // v9.3-alpha2.4: never stall Core1/UI for the default 1 second

  // Recovery gesture is sampled before diagnostics can disable Serial. Holding
  // NAV during reset for 0.7 s always bypasses the saved platform and opens the
  // minimal Mode Center, even after an early-boot crash loop.
  pinMode(IQ200_ENC_NAV_SW, INPUT_PULLUP);
  pinMode(IQ200_ENC_VOL_SW, INPUT_PULLUP);
  delay(30);
  bool forceCenterRequested = false;
  if (digitalRead(IQ200_ENC_NAV_SW) == LOW) {
    const uint32_t heldSince = millis();
    while (digitalRead(IQ200_ENC_NAV_SW) == LOW && millis() - heldSince < 900U) {
      delay(10);
    }
    forceCenterRequested = millis() - heldSince >= 700U;
  }

  diagnosticsOutputEnabled = diagnosticsLoadEnabled();
  processLogEnabled = diagnosticsOutputEnabled;
  if (!diagnosticsOutputEnabled) {
    esp_log_level_set("*", ESP_LOG_NONE);
    Serial.flush();
    Serial.end();
  }
  delay(2500);

  activeMode = modeManager.begin(forceCenterRequested);
  mirrorActiveMode();

  if (activeMode == IQ200_MODE_CENTER) {
    Serial.println();
    Serial.println("========================================");
    Serial.println("IQ200 OS v9.9-alpha8 COMMERCIAL WEBRADIO UI");
    Serial.println("Clean boot: WiFi / SD / audio are OFF");
    Serial.println("========================================");
    const bool displayOk = display.begin();
    Serial.printf("Display init: %d\n", displayOk ? 1 : 0);
    // Render the first frame synchronously on the Arduino setup task. Some
    // LovyanGFX/SPI combinations initialize successfully but do not show the
    // first transaction when it originates immediately from a newly-created
    // FreeRTOS task. The UI task only owns input/redraws after this point.
    delay(120);  // let ILI9488 leave reset/sleep before the first SPI frame
    Serial.println("[MODE_UI] first frame begin");
    modeCenterUi.begin();
    Serial.println("[MODE_UI] first frame ready");
    const BaseType_t modeTaskRc = xTaskCreatePinnedToCore(
      modeCenterTask,
      "mode_center_ui",
      6144,
      nullptr,
      2,
      &modeUiTaskHandle,
      1
    );
    Serial.printf("[TASK] mode_center_ui create=%s stack=6144\n",
                  modeTaskRc == pdPASS ? "OK" : "FAIL");
    return;
  }

  if (activeMode == IQ200_MODE_WEBRADIO) {
    // WebRadio clean boot: only its network, HTTP, stream and lightweight UI
    // services are initialized. Local SD/database/decoder/artwork objects stay
    // dormant and AudioEngine never installs a competing I2S driver.
    commandManager.begin();
    watchdogService.begin();
    connectivityManager.begin(runtimeState);
    radioService.begin(runtimeState, iqPlayerCore, audioEngine);
    webServerService.begin(runtimeState, storageService, commandManager, playlist, radioService);
    stabilityService.begin(runtimeState);
    commercialPolishService.begin(runtimeState);
    runtimeState.volumePercent = 8;
    runtimeState.bootReadyMs = millis();
    strncpy(runtimeState.bootPhase, "WEBRADIO", sizeof(runtimeState.bootPhase) - 1);
    runtimeState.bootPhase[sizeof(runtimeState.bootPhase) - 1] = 0;

    Serial.println();
    Serial.println("========================================");
    Serial.println("IQ200 OS v9.9-alpha8 COMMERCIAL WEBRADIO UI CLEAN BOOT");
    Serial.println("Local Player / SD / DB / Artwork are OFF");
    Serial.println("========================================");
    const bool displayOk = display.begin();
    Serial.printf("Display init: %d\n", displayOk ? 1 : 0);
    delay(120);  // same deterministic first-frame settle as Mode Center
    Serial.println("[WEBRADIO_UI] first frame begin");
    webRadioModeUi.begin();
    Serial.println("[WEBRADIO_UI] first frame ready");

    connectivityManager.boot();
    startDedicatedWebTask();
    const BaseType_t coreRc = xTaskCreatePinnedToCore(
      webRadioCoreTask,
      "webradio_core",
      6144,
      nullptr,
      1,
      &core0TaskHandle,
      0
    );
    const BaseType_t uiRc = xTaskCreatePinnedToCore(
      webRadioUiTask,
      "webradio_ui",
      6144,
      nullptr,
      2,
      &core1TaskHandle,
      1
    );
    Serial.printf("[TASK] webradio_core=%s webradio_ui=%s\n",
                  coreRc == pdPASS ? "OK" : "FAIL",
                  uiRc == pdPASS ? "OK" : "FAIL");
    return;
  }

  commandManager.begin();
  const bool bbOk = blackBox.begin(512);
  Serial.printf("[BB] init=%d storage=%s capacity=%u\n", bbOk ? 1 : 0, blackBox.inPsram() ? "PSRAM" : "DRAM", (unsigned)blackBox.capacity());

  systemService.begin();
  watchdogService.begin();
  coreOS.begin();
  otaService.begin();
  settingsService.begin();
  wifiProfiles.begin();
  serviceManager.begin(&runtimeState);
  mediaEngine.begin(runtimeState);
  mediaCore.begin(runtimeState, queueManager);
  iqPlayerCore.begin(runtimeState, audioEngine, wavPlayer, flacPlayer, mp3Player);
  resumeEngine.begin(runtimeState);
  mediaPipeline.begin(runtimeState);
  databaseService.begin(runtimeState, mediaDb, libraryManager, favoriteManager, queueManager, resumeEngine);
  smartResumeService.begin(runtimeState, databaseService, queueManager, audioEngine);
  audioEngine.setEqualizer(runtimeState.eqEnabled, runtimeState.eqBassDb, runtimeState.eqMidDb, runtimeState.eqTrebleDb, runtimeState.eqPreset);
  connectivityManager.begin(runtimeState);
  webServerService.begin(runtimeState, storageService, commandManager, playlist, radioService);
  connectivityManager.boot();  // v9.8-alpha9: AutoConnect / fallback AP / captive portal
  stabilityService.begin(runtimeState);
  commercialPolishService.begin(runtimeState);
  favoriteManager.begin("/iq200/db/favorites/favorites.db");
  scanService.begin(runtimeState, storageService, mediaDb, playlist, wavPlayer, libraryManager, eventBus);
  // v8.2.5: ScanService is ready before commands can schedule background scans.
  // v8.2.1: attach progress buffers now, but open /iq200/db only after SD mount.
  runtimeState.bootReadyMs = millis();
  strncpy(runtimeState.bootPhase, "INIT", sizeof(runtimeState.bootPhase) - 1);
  runtimeState.bootPhase[sizeof(runtimeState.bootPhase) - 1] = 0;
  mediaDb.attachProgress((volatile int*)&runtimeState.scanProgress, (volatile int*)&runtimeState.scanFiles, (volatile int*)&runtimeState.scanTracks, runtimeState.scanMessage, sizeof(runtimeState.scanMessage), (volatile int*)&runtimeState.scanDirs, (volatile int*)&runtimeState.scanMp3, (volatile int*)&runtimeState.scanFlac, (volatile int*)&runtimeState.scanWav, runtimeState.scanCurrentPath, sizeof(runtimeState.scanCurrentPath));
  // v8.2.1 Build Verify: do not inject /test.wav into a real SD library.
  runtimeState.playlistCount = playlist.size();
  runtimeState.playlistIndex = playlist.index();

  Serial.println();
  Serial.println("========================================");
  Serial.println("IQ200 OS v9.9-alpha8 LOCAL PLAYER CLEAN BOOT");
  Serial.println("WebRadio decoder task is OFF");
  Serial.println("Core0: Web task + services + RT audio task");
  Serial.println("Core1: CoreOS UI");
  Serial.println("========================================");
  Serial.printf("Setup core: %d\n", xPortGetCoreID());
  Serial.printf("Flash: %lu\n", (unsigned long)ESP.getFlashChipSize());
  Serial.printf("PSRAM: %lu free=%lu\n", (unsigned long)ESP.getPsramSize(), (unsigned long)ESP.getFreePsram());

  bool ok = display.begin();
  Serial.printf("Display init: %d\n", ok);

  audioEngine.begin();
  audioEngine.setVolume(8);
  runtimeState.volumePercent = 8;

  iqEventQueue = xQueueCreate(12, sizeof(IQEvent));
  Serial.printf("Event queue: %s\n", iqEventQueue ? "OK" : "FAIL");
  runtimeState.sdMountRequest = true;  // v8.2.6: Core0 mounts SD before DB fast-load.
  messageBroker.begin(&iqEventQueue);
  eventBus.begin(runtimeState, messageBroker);

  // WebServer must not share the worker that performs blocking local
  // startup (decoder stop, Artwork SD load/render wait and FLAC open). Priority
  // 2 stays below RT audio (5) and shares fairly with the WebRadio task (2).
  startDedicatedWebTask();

  xTaskCreatePinnedToCore(
    core0Worker,
    "core0_worker",
    8192,
    nullptr,
    1,
    &core0TaskHandle,
    0
  );

  xTaskCreatePinnedToCore(
    core1UiTask,
    "core1_ui",
    12288,
    nullptr,
    2,
    &core1TaskHandle,
    1
  );

  Serial.println("Commands: type h (or ?) for the complete categorized command list");
  Serial.println("Aliases : st=status, hl=health, rb=reboot, now=player, mi/info=minfo, m=media, p=play, s=stop, q=queue, fav=favorites, qshuffle=shuffle, repeat off/one/all, ls=index, scan/rescan=dbscan, update/upd=dbupdate, list=pl");
  Serial.println("Media Core: v9.7-alpha2 streaming MP3 (no full-file scan) + artwork cache fix");
}

void loop() {
  // Main Arduino loop is intentionally idle.
  // All work is split between pinned FreeRTOS tasks:
  // Core0 = background services
  // Core1 = UI/display/encoders
  vTaskDelay(pdMS_TO_TICKS(1000));
}
