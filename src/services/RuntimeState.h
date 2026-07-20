#pragma once
#include <Arduino.h>

struct RuntimeState {
  // v9.9-alpha7 clean-boot mode isolation. -1 means no switch requested.
  volatile uint8_t systemMode = 0;
  volatile int8_t modeSwitchRequest = -1;
  volatile bool modeBootHealthy = false;
  volatile uint8_t modeEarlyBootFailures = 0;
  volatile uint32_t modeStartedMs = 0;
  char systemModeName[24] = "MODE_CENTER";

  volatile bool wifiScanRequest = false;
  volatile bool wifiScanBusy = false;
  volatile int wifiNetworks = -1;

  volatile bool sdMountRequest = false;
  volatile bool sdBusy = false;
  volatile bool sdOk = false;
  volatile uint64_t sdMB = 0;

  volatile bool audioToneRequest = false;
  volatile bool audioBusy = false;

  volatile uint32_t core0Loops = 0;
  volatile uint32_t core1Loops = 0;

  volatile bool uiDirty = false;
  volatile uint32_t lastCore0Event = 0;
  char lastMessage[64] = "READY";

  volatile uint32_t core0HeapFree = 0;
  volatile uint32_t core1HeapFree = 0;
  volatile uint32_t core0StackHighWater = 0;
  volatile uint32_t core1StackHighWater = 0;
  volatile uint32_t eventQueueDrops = 0;

  volatile uint32_t core0AgeMs = 0;
  volatile uint32_t core1AgeMs = 0;
  volatile bool core0Ok = true;
  volatile bool core1Ok = true;

  volatile bool schedulerMode = true;
  volatile uint32_t audioTicks = 0;
  volatile uint32_t sdTicks = 0;
  volatile uint32_t wifiTicks = 0;
  volatile uint32_t guiTicks = 0;
  volatile uint32_t inputTicks = 0;
  volatile uint32_t animationTicks = 0;

  volatile uint32_t appSwitches = 0;
  char currentApp[24] = "Home";

  volatile uint32_t serviceTicks = 0;
  volatile uint32_t rendererFrames = 0;
  volatile uint32_t dirtyFrames = 0;
  volatile uint32_t partialFrames = 0;
  volatile uint32_t fullFrames = 0;
  // v9.2-alpha17: independent Player UI scheduler diagnostics.
  volatile uint32_t playerVuTicks = 0;
  volatile uint32_t playerVuDraws = 0;
  volatile uint32_t playerProgressTicks = 0;
  volatile uint32_t playerProgressDraws = 0;
  volatile uint32_t playerMetaDraws = 0;

  volatile bool indexRequest = false;
  volatile bool indexBusy = false;
  volatile int fileIndexCount = 0;

  volatile bool audioStreamReady = false;
  volatile bool audioPlaying = false;
  volatile bool wavOpenRequest = false;
  volatile bool wavPlayRequest = false;
  volatile bool wavStopRequest = false;
  volatile bool playWavFallbackRequest = false; // v9.1.1: jump to next indexed WAV for hardware audio test
  volatile bool playWavTestMode = false; // v9.1.1: playwav must not overwrite normal SmartResume
  volatile bool wavValid = false;
  volatile uint32_t wavSampleRate = 0;
  volatile uint16_t wavChannels = 0;
  volatile uint32_t wavDataSize = 0;
  volatile uint32_t wavPlayedBytes = 0;
  volatile int wavProgress = 0;
  volatile uint8_t wavVuLeft = 0;
  volatile uint8_t wavVuRight = 0;
  volatile bool rtAudioTaskRunning = false;
  volatile uint32_t audioBytes = 0;
  volatile uint32_t audioUnderruns = 0;
  volatile uint32_t audioShortWrites = 0;
  volatile uint32_t audioRtLoops = 0;
  volatile uint32_t audioLastChunkBytes = 0;
  volatile uint32_t audioTaskStackHighWater = 0;
  volatile uint8_t audioHealth = 100;

  // v9.1.1 Next Track Engine diagnostics. These are intentionally
  // lightweight mirrors: they let diag/UI see pipeline pressure without
  // adding blocking instrumentation to the RT path.
  volatile uint8_t playerRingFillPct = 0;
  volatile uint8_t playerCacheHitPct = 0;
  volatile uint32_t playerCacheHits = 0;
  volatile uint32_t playerCacheMisses = 0;
  volatile uint32_t playerReadAheadBytes = 0;
  volatile uint32_t playerSdRetries = 0;
  volatile uint32_t playerSdErrors = 0;
  volatile uint8_t playerDecoderLoadPct = 0;
  char playerStateName[16] = "IDLE";


  // v7.2 Media Foundation: codec-neutral state mirrored from RT audio path.
  char mediaTitle[64] = "";
  char mediaPath[192] = "";
  volatile uint8_t mediaCodec = 0;   // MEDIA_CODEC_NONE
  volatile uint8_t mediaState = 0;   // MEDIA_STATE_READY
  volatile uint32_t mediaSampleRate = 0;
  volatile uint16_t mediaChannels = 0;
  volatile uint8_t mediaBits = 16;
  volatile uint32_t mediaDataSize = 0;
  volatile uint32_t mediaPlayedBytes = 0;
  volatile uint8_t mediaProgress = 0;
  volatile uint8_t mediaVuLeft = 0;
  volatile uint8_t mediaVuRight = 0;
  volatile uint8_t mediaBufferHealth = 100;
  volatile uint32_t mediaUnderruns = 0;
  volatile uint32_t mediaShortWrites = 0;


  // v7.3 Media Framework: playlist/control bridge for UI -> Core0.
  volatile bool playlistAddTestRequest = false;
  volatile bool playlistNextRequest = false;
  volatile bool webTrackPlayRequest = false;
  volatile int webTrackPlayIndex = -1;
  volatile bool playlistNextAutoPlayRequest = false; // legacy/manual handoff compatibility
  volatile bool playlistPrevRequest = false;

  // v9.5-alpha3 Navigation Engine 2.0: Core1 only accumulates direction;
  // Core0 applies the final selection and commits playback after an idle delay.
  volatile int32_t navPendingDelta = 0;
  volatile uint32_t navLastInputMs = 0;
  volatile uint16_t navCommitDelayMs = 450;
  volatile bool navPreviewEnabled = true;
  volatile bool navPreviewActive = false;
  volatile bool navCommitPending = false;
  volatile bool navAutoPlayAfterCommit = false;
  // v9.8-alpha5: explicit manual-navigation ownership of decoder STOP.
  // Prevents a stopped track at 100% from being mistaken for natural EOF.
  volatile bool navManualControlPending = false;
  volatile uint32_t navPreviewMoves = 0;
  volatile uint32_t navCommits = 0;

  volatile bool playlistClearRequest = false;
  volatile bool playlistScanRequest = false;
  volatile bool playlistListRequest = false;
  volatile int playlistCount = 0;
  volatile int playlistIndex = 0;
  char playlistCurrent[192] = "";

  // v9.1.1 Next Track Engine: atomic Core0 handoff state.
  volatile bool trackHandoffActive = false;
  volatile bool trackHandoffAutoPlay = false;
  volatile uint32_t trackHandoffCount = 0;
  volatile uint32_t trackHandoffTimeouts = 0;
  volatile uint32_t trackHandoffLastMs = 0;
  char trackHandoffState[20] = "IDLE";

  volatile bool dbScanRequest = false;
  volatile bool dbScanDeferred = false; // v9.2-alpha2: wait for playback to stop before full SD scan
  volatile bool dbScanBusy = false;
  volatile bool scanLock = false;
  volatile uint8_t scanProgress = 0;
  volatile uint32_t scanStartMs = 0;
  volatile uint32_t scanElapsedMs = 0;
  volatile int scanFiles = 0;
  volatile int scanTracks = 0;
  volatile int scanDirs = 0;
  volatile int scanMp3 = 0;
  volatile int scanFlac = 0;
  volatile int scanWav = 0;
  char scanMessage[64] = "READY";
  char scanCurrentPath[192] = "/";
  volatile bool dbLoadRequest = false;
  volatile bool dbInfoRequest = false;
  volatile bool dbClearRequest = false;
  volatile int dbTrackCount = 0;
  volatile int dbVolumeCount = 0;
  volatile int dbArtCount = 0;
  volatile int dbMp3Count = 0;
  volatile int dbFlacCount = 0;
  volatile int dbWavCount = 0;
  volatile bool dbTestRequest = false;
  volatile int dbTestErrors = 0;
  volatile bool dbFindRequest = false;
  volatile int dbFindLimit = 16;
  char dbFindQuery[64] = "";
  volatile int dbFindResults = 0;
  volatile uint32_t dbFindLastMs = 0;


  // v8.0.3 Incremental Update Engine foundation.
  volatile bool dbUpdateRequest = false;
  volatile bool dbUpdateBusy = false;
  volatile uint32_t dbUpdateLastMs = 0;
  volatile int dbUpdateLoaded = 0;


  // v8.0.1 Media Core / Queue Manager bridge.
  volatile bool queueListRequest = false;
  volatile bool queueAddCurrentRequest = false;
  volatile bool queueClearRequest = false;
  volatile bool queueNextRequest = false;
  volatile bool queuePrevRequest = false;
  volatile bool queueSaveRequest = false;
  volatile bool queueLoadRequest = false;
  volatile bool queueShuffleToggleRequest = false;
  volatile bool queueModeInfoRequest = false;
  volatile int queueRepeatSetRequest = -1;
  volatile bool queueShuffleSmart = false;
  volatile int queueRepeatMode = 2;
  volatile int queueCount = 0;
  volatile int queueIndex = 0;
  char queueCurrent[192] = "";



  // v8.0.2 Event Bus + Resume Engine diagnostics.
  volatile uint32_t eventBusPosts = 0;
  volatile uint32_t eventBusDrops = 0;
  volatile uint32_t mediaEventCount = 0;
  volatile uint32_t queueEventCount = 0;
  volatile uint32_t dbEventCount = 0;
  volatile uint32_t scanEventCount = 0;

  volatile bool resumeSaveRequest = false;
  volatile bool resumeLoadRequest = false;
  volatile bool resumeInfoRequest = false;
  volatile bool resumeClearRequest = false;
  volatile uint32_t resumeLastSaveMs = 0;
  volatile uint32_t resumeSaveCount = 0;
  char resumeLastPath[192] = "";
  // v8.2.5 Smart Resume: automatic SD-backed restore/save state.
  volatile bool resumeAutoEnabled = true;
  volatile bool resumeRestored = false;
  volatile bool resumeAutoSaveRequest = false;
  volatile uint32_t resumeLastAutoSaveMs = 0;
  volatile uint32_t resumeRestoreCount = 0;
  volatile uint32_t resumeLoadedPositionBytes = 0;
  volatile uint8_t resumeLoadedProgress = 0;
  volatile int resumeLoadedVolume = 42;
  // Saved active-playlist cursor loaded before PlaylistManager is synchronized.
  volatile int resumeLoadedPlaylistIndex = -1;
  volatile int resumeLoadedPlaylistCount = 0;
  volatile int resumeLoadedQueueIndex = 0;
  volatile int resumeLoadedQueueCount = 0;
  volatile bool resumeLoadedShuffle = false;
  volatile int resumeLoadedRepeat = 2;
  volatile int volumePercent = 8;

  // v9.1.1 Next Track Engine: optional boot playback of the track-only
  // SmartResume target. It never restores position; playback starts at 0%.
  volatile bool autoplayEnabled = false;
  volatile bool autoplayInfoRequest = false;
  volatile bool autoplayOnRequest = false;
  volatile bool autoplayOffRequest = false;
  volatile bool autoplayPending = false;
  volatile bool autoplayTried = false;
  volatile uint32_t autoplayReadyMs = 0;
  volatile uint32_t autoplayDelayMs = 1800;
  volatile uint32_t autoplayStartCount = 0;
  volatile uint32_t autoplaySkipCount = 0;
  volatile uint32_t autoplayRetryCount = 0;
  volatile uint32_t autoplayMaxRetries = 6;
  volatile uint32_t autoplayLastDecisionMs = 0;
  char autoplayStatus[32] = "OFF";


  // v8.0.5 Media Pipeline diagnostics.
  volatile bool pipelineInfoRequest = false;
  volatile uint32_t pipelineTicks = 0;
  volatile uint32_t pipelineDropped = 0;
  volatile uint32_t pipelineFileQueueDepth = 0;
  volatile uint32_t pipelineFileQueueSize = 64;
  volatile uint32_t pipelineMetaQueueDepth = 0;
  volatile uint32_t pipelineMetaQueueSize = 32;
  volatile uint32_t pipelineDbQueueDepth = 0;
  volatile uint32_t pipelineDbQueueSize = 32;
  volatile uint32_t pipelineParserOk = 1;
  volatile uint32_t pipelineWriterOk = 1;
  volatile uint32_t pipelineAlbumArtOk = 1;


  // v8.1.5 Favorites UI / SD-backed favorites.db.
  volatile bool favoriteListRequest = false;
  volatile bool favoriteAddCurrentRequest = false;
  volatile bool favoriteClearRequest = false;
  volatile bool favoriteSaveRequest = false;
  volatile bool favoriteLoadRequest = false;
  volatile int favoriteCount = 0;
  volatile uint32_t favoriteLastMs = 0;
  char favoriteLastPath[192] = "";



  // v8.1.6 Media Library Complete / index browser foundation.
  volatile bool libraryBuildRequest = false;
  volatile bool libraryBuildForce = false;
  volatile bool libraryStatsRequest = false;
  volatile bool artistListRequest = false;
  volatile bool albumListRequest = false;
  volatile bool genreListRequest = false;
  volatile bool folderListRequest = false;
  volatile bool recentListRequest = false;
  volatile bool mostListRequest = false;
  volatile int libraryArtistCount = 0;
  volatile int libraryAlbumCount = 0;
  volatile int libraryGenreCount = 0;
  volatile int libraryFolderCount = 0;
  volatile int libraryRecentCount = 0;
  volatile int libraryMostCount = 0;
  volatile uint32_t libraryLastMs = 0;
  char libraryLastView[32] = "Library";

    volatile bool wifiProfileSaved = false;

  // v8.2.0 Core Stabilization diagnostics and runtime policy.
  volatile bool coreStabilizationMode = true;
  volatile bool asyncScanPolicy = true;
  volatile bool partialRenderPolicy = true;
  volatile uint32_t bootReadyMs = 0;
  volatile uint32_t serviceHeartbeatMs = 0;
  volatile uint32_t serviceWatchdogWarnings = 0;
  volatile uint32_t sdLatencyLastMs = 0;
  volatile uint32_t sdLatencyMaxMs = 0;
  volatile uint32_t diagLastShownMs = 0;
  char bootPhase[32] = "BOOT";


  // v9.8-alpha7 Artwork Engine requests.
  volatile bool artworkReloadRequest = false;
  volatile uint32_t artworkRenderRequestGeneration = 0;
  volatile uint32_t artworkRenderedGeneration = 0;

  // v8.6 Web UI / OTA Base / Connectivity runtime state.
  volatile bool connectivityReady = false;
  volatile uint32_t networkTicks = 0;
  volatile bool wifiConnected = false;
  volatile bool wifiApMode = false;
  volatile bool wifiStaMode = false;
  volatile bool netApRequest = false;
  volatile bool netOffRequest = false;
  volatile bool wifiStaConnectRequest = false;
  volatile bool wifiApStaConnectRequest = false;
  volatile bool wifiDisconnectRequest = false;
  volatile bool wifiScanNowRequest = false;
  volatile bool wifiLoadRequest = false;
  volatile bool wifiSaveRequest = false;
  volatile bool wifiStatusRequest = false;
  volatile bool wifiForgetRequest = false;
  volatile bool wifiAutoOnRequest = false;
  volatile bool wifiAutoOffRequest = false;
  volatile bool wifiFallbackOnRequest = false;
  volatile bool wifiFallbackOffRequest = false;
  volatile bool wifiBootRequest = false;
  volatile bool wifiAutoConnect = true;
  volatile bool wifiFallbackAp = true;
  volatile uint32_t wifiReconnectCount = 0;
  volatile int wifiRssi = 0;
  char wifiIp[24] = "0.0.0.0";
  char wifiSsid[33] = "";
  char wifiSavedSsid[33] = "";
  char wifiPendingSsid[33] = "";
  char wifiPendingPassword[65] = "";
  char wifiStatus[24] = "OFF";

  volatile bool webEnabled = false;
  volatile bool webRunning = false;
  volatile uint16_t webPort = 80;
  volatile uint32_t webRequests = 0;
  volatile uint32_t webLastRequestMs = 0;
  volatile uint32_t webTicks = 0;
  volatile uint32_t webTickGapLastMs = 0;
  volatile uint32_t webTickGapMaxMs = 0;
  volatile bool webTaskRunning = false;
  volatile uint32_t webTaskLoops = 0;
  volatile uint32_t webTaskStackHighWater = 0;
  volatile bool webEnableRequest = false;
  volatile bool webDisableRequest = false;
  volatile bool webInfoRequest = false;
  char webIp[24] = "0.0.0.0";
  char webStatus[24] = "OFF";

  volatile bool otaInfoRequest = false;
  volatile bool otaSdRequest = false;
  volatile bool otaBusy = false;
  volatile int otaProgress = 0;
  char otaStatus[32] = "READY";

  volatile bool radioInfoRequest = false;
  volatile bool radioStopRequest = false;
  volatile bool radioPlaying = false;
  // WebRadio VU is sampled only by webradio_rt and consumed lock-free by UI/Web.
  // ESP32-audioI2S reports two unsigned 8-bit channel levels packed in uint16_t.
  volatile uint8_t radioVuLeft = 0;
  volatile uint8_t radioVuRight = 0;
  volatile uint32_t radioVuTicks = 0;
  volatile uint8_t radioVuSegments = 24;
  volatile uint8_t radioVuFps = 20;
  volatile bool radioVuPeak = true;
  volatile uint16_t radioVuHoldMs = 450;
  volatile uint8_t radioVuDecay = 5;
  volatile uint8_t radioVuStyle = 2; // 0=line, 1=thin, 2=blocks, 3=dots, 4=neon, 5=center
  volatile uint16_t radioVuGain = 100;
  volatile uint8_t radioVuGate = 2;
  volatile uint8_t displayBrightness = 255;
  char activeTheme[16] = "bluepro";
  // Marks a local-decoder stop initiated by WebRadio takeover so the local
  // Auto Next state machine never mistakes it for a natural EOF.
  volatile bool radioTakeoverStop = false;
  char radioStatus[24] = "READY";
  char radioStation[96] = "";
  char radioUrl[241] = "";
  char radioTitle[128] = "";
  char radioError[128] = "";
  volatile uint32_t radioCommandsQueued = 0;
  volatile uint32_t radioCommandsDropped = 0;
  volatile uint32_t radioConnectAttempts = 0;
  volatile uint32_t radioReconnects = 0;
  volatile uint32_t radioUnsupportedStreams = 0;
  volatile uint32_t radioTaskStackHighWater = 0;


  // v8.7.x/v8.9 burn-in / recovery diagnostics.
  volatile bool stabilityEnabled = false;
  volatile bool burninActive = false;
  volatile uint32_t stabilityStartedMs = 0;
  volatile uint32_t stabilityUptimeMs = 0;
  volatile uint32_t stabilityTicks = 0;
  volatile uint32_t stabilityRecoveries = 0;
  volatile uint32_t stabilityWatchdogWarnings = 0;
  volatile uint32_t stabilitySdErrors = 0;
  volatile uint32_t stabilitySdLatencyMaxMs = 0;
  volatile uint32_t stabilityMinHeap = 0;
  volatile uint32_t stabilityMinPsram = 0;
  volatile uint32_t stabilityLeakWarnings = 0;
  volatile uint32_t burninStartedMs = 0;
  volatile bool stabilityInfoRequest = false;
  volatile bool burninStartRequest = false;
  volatile bool burninStopRequest = false;
  char stabilityStatus[24] = "READY";


  // v8.9 Release Candidate: theme/icons/help polish runtime state.
  volatile bool commercialPolishEnabled = false;
  volatile uint8_t commercialThemeId = 0;
  volatile uint8_t commercialIconSetId = 0;
  volatile bool commercialHelpClean = false;
  volatile uint32_t commercialPolishTicks = 0;
  volatile bool commercialInfoRequest = false;
  char commercialThemeName[32] = "Enterprise Dark";
  char commercialUiStatus[24] = "READY";


  // v9.8-alpha31: lightweight three-band DSP equalizer state.
  bool eqEnabled = true;
  int8_t eqBassDb = 0;
  int8_t eqMidDb = 0;
  int8_t eqTrebleDb = 0;
  char eqPreset[16] = "flat";
};
