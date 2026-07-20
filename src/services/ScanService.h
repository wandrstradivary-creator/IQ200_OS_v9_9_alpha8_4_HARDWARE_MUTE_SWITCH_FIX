#pragma once
#include <Arduino.h>
#include <SD.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "RuntimeState.h"
#include "StorageService.h"
#include "MediaDatabase.h"
#include "PlaylistManager.h"
#include "WavPlayerService.h"
#include "LibraryManager.h"
#include "EventBus.h"
#include "MediaFramework.h"

// IQ200 OS v8.2.5 ScanService
// Dedicated SD database scan task. Core0 only schedules the scan; the heavy
// rebuild runs in its own FreeRTOS task and yields inside MediaDatabase, so the
// UI/encoder task on Core1 remains responsive during large-library scans.
class ScanService {
public:
  void begin(RuntimeState& state,
             StorageService& storage,
             MediaDatabase& db,
             PlaylistManager& playlist,
             WavPlayerService& wav,
             LibraryManager& library,
             EventBus& bus) {
    _rt = &state;
    _storage = &storage;
    _db = &db;
    _playlist = &playlist;
    _wav = &wav;
    _library = &library;
    _bus = &bus;
  }

  bool busy() const {
    return _task != nullptr || (_rt && (_rt->scanLock || _rt->dbScanBusy));
  }

  bool requestFullScan(const char* reason = "SD scan: background...") {
    if (!_rt || !_storage || !_db || !_playlist || !_wav || !_library) return false;
    if (busy()) {
      Serial.println("[SCANSVC] ignored: scan already running");
      post(EVT_MONITOR, _rt->scanProgress, _rt->scanFiles, "Scan already running");
      return false;
    }

    resetState(reason);
    BaseType_t ok = xTaskCreatePinnedToCore(
      &ScanService::taskThunk,
      "scan_service",
      12288,
      this,
      1,
      &_task,
      0
    );

    if (ok != pdPASS) {
      _task = nullptr;
      finishState("Scan task create failed", false);
      post(EVT_ERROR, 0, 0, "Scan task create failed");
      return false;
    }

    Serial.println("[SCANSVC] background scan task started on Core0");
    post(EVT_MONITOR, 0, 0, reason);
    return true;
  }

  void tick() {
    if (_rt && _rt->scanLock) {
      _rt->scanElapsedMs = millis() - _rt->scanStartMs;
    }
  }

private:
  RuntimeState* _rt = nullptr;
  StorageService* _storage = nullptr;
  MediaDatabase* _db = nullptr;
  PlaylistManager* _playlist = nullptr;
  WavPlayerService* _wav = nullptr;
  LibraryManager* _library = nullptr;
  EventBus* _bus = nullptr;
  TaskHandle_t _task = nullptr;

  static void taskThunk(void* arg) {
    static_cast<ScanService*>(arg)->runTask();
    vTaskDelete(nullptr);
  }

  void post(IQEventType type, int value, uint64_t value64, const char* msg) {
    if (_bus && !_bus->post(type, value, value64, msg) && _rt) _rt->eventQueueDrops++;
  }

  void safeCopy(char* dst, size_t len, const char* src) {
    if (!dst || len == 0) return;
    strncpy(dst, src ? src : "", len - 1);
    dst[len - 1] = 0;
  }

  void resetState(const char* msg) {
    _rt->scanLock = true;
    _rt->dbScanBusy = true;
    _rt->scanProgress = 0;
    _rt->scanFiles = 0;
    _rt->scanTracks = 0;
    _rt->scanDirs = 0;
    _rt->scanMp3 = 0;
    _rt->scanFlac = 0;
    _rt->scanWav = 0;
    _rt->scanStartMs = millis();
    _rt->scanElapsedMs = 0;
    safeCopy(_rt->scanCurrentPath, sizeof(_rt->scanCurrentPath), "Waiting...");
    safeCopy(_rt->scanMessage, sizeof(_rt->scanMessage), msg);
    safeCopy(_rt->lastMessage, sizeof(_rt->lastMessage), msg);
    _rt->uiDirty = true;
    Serial.println("========================================");
    Serial.println("[SCANSVC] SD media scan scheduled");
    Serial.println("[SCANSVC] UI/encoder remain active during scan");
    Serial.println("========================================");
  }

  void finishState(const char* msg, bool ok) {
    _rt->scanProgress = ok ? 100 : _rt->scanProgress;
    _rt->scanElapsedMs = millis() - _rt->scanStartMs;
    safeCopy(_rt->scanMessage, sizeof(_rt->scanMessage), msg);
    safeCopy(_rt->lastMessage, sizeof(_rt->lastMessage), msg);
    _rt->scanLock = false;
    _rt->dbScanBusy = false;
    _rt->uiDirty = true;
    Serial.println("========================================");
    Serial.printf("[SCANSVC] %s: tracks=%d volumes=%d art=%d time=%lu ms\n",
                  ok ? "complete" : "failed", _rt->dbTrackCount, _rt->dbVolumeCount,
                  _rt->dbArtCount, (unsigned long)_rt->scanElapsedMs);
    Serial.println("========================================");
  }

  void runTask() {
    Serial.printf("[SCANSVC] task running on core %d\n", xPortGetCoreID());

    // v9.2-alpha2: never terminate playback as a side effect of scan.
    // main.cpp queues full scans until IQPlayerCore is idle. This final guard
    // prevents a race if WAV playback starts after the request was accepted.
    if (_wav->isPlaying() || _wav->isTaskRunning()) {
      Serial.println("[SCANSVC] aborted: audio became active before scan start");
      finishState("Scan deferred: audio active", false);
      post(EVT_MONITOR, 0, 0, "Scan deferred: audio active");
      _task = nullptr;
      return;
    }

    _rt->audioPlaying = false;
    _rt->mediaState = MEDIA_STATE_LOADING;

    if (!_storage->ok && !_storage->mount()) {
      finishState("SD mount failed", false);
      post(EVT_ERROR, 0, 0, "SD mount failed");
      _task = nullptr;
      return;
    }

    if (!_db->begin()) {
      finishState("DB begin failed", false);
      post(EVT_ERROR, 0, 0, "DB begin failed");
      _task = nullptr;
      return;
    }

    _db->attachProgress((volatile int*)&_rt->scanProgress,
                        (volatile int*)&_rt->scanFiles,
                        (volatile int*)&_rt->scanTracks,
                        _rt->scanMessage, sizeof(_rt->scanMessage),
                        (volatile int*)&_rt->scanDirs,
                        (volatile int*)&_rt->scanMp3,
                        (volatile int*)&_rt->scanFlac,
                        (volatile int*)&_rt->scanWav,
                        _rt->scanCurrentPath, sizeof(_rt->scanCurrentPath));

    Serial.println("[SCANSVC] rebuild begin: indexed SD database");
    int tracks = _db->rebuild();
    _rt->dbTrackCount = tracks;
    _rt->dbVolumeCount = _db->volumes();
    _rt->dbArtCount = _db->art();
    _rt->dbMp3Count = _rt->scanMp3;
    _rt->dbFlacCount = _rt->scanFlac;
    _rt->dbWavCount = _rt->scanWav;

    int loaded = _db->loadPlaylist(*_playlist);
    _rt->fileIndexCount = tracks;

    if (_playlist->size() > 0) {
      String cur = _playlist->currentTrack();
      safeCopy(_rt->mediaPath, sizeof(_rt->mediaPath), cur.c_str());
      safeCopy(_rt->playlistCurrent, sizeof(_rt->playlistCurrent), cur.c_str());
      const char* slash = strrchr(_rt->mediaPath, '/');
      safeCopy(_rt->mediaTitle, sizeof(_rt->mediaTitle), slash ? slash + 1 : _rt->mediaPath);
      _rt->mediaCodec = mediaCodecFromPath(_rt->mediaPath);
    } else {
      safeCopy(_rt->mediaPath, sizeof(_rt->mediaPath), "");
      safeCopy(_rt->playlistCurrent, sizeof(_rt->playlistCurrent), "");
      safeCopy(_rt->mediaTitle, sizeof(_rt->mediaTitle), "No media");
      _rt->mediaCodec = MEDIA_CODEC_NONE;
      _rt->mediaState = MEDIA_STATE_STOPPED;
    }

    _rt->playlistCount = _playlist->size();
    _rt->playlistIndex = _playlist->index();

    // v9.8-alpha23: a successful scan must leave the player in a clean,
    // playable READY state. The scan previously set MEDIA_STATE_LOADING at
    // entry and never restored it, so Play could appear accepted while the
    // player state machine still looked busy/loading.
    _rt->wavPlayRequest = false;
    _rt->wavStopRequest = false;
    _rt->audioBusy = false;
    _rt->audioPlaying = false;
    _rt->rtAudioTaskRunning = false;
    _rt->trackHandoffActive = false;
    _rt->trackHandoffAutoPlay = false;
    _rt->playlistNextRequest = false;
    _rt->playlistNextAutoPlayRequest = false;
    _rt->playlistPrevRequest = false;
    _rt->navManualControlPending = false;
    _rt->navPreviewActive = false;
    _rt->navCommitPending = false;
    _rt->navPendingDelta = 0;
    _rt->mediaProgress = 0;
    _rt->mediaPlayedBytes = 0;
    _rt->wavProgress = 0;
    _rt->wavPlayedBytes = 0;
    _rt->mediaVuLeft = 0;
    _rt->mediaVuRight = 0;
    if (_playlist->size() > 0) {
      _rt->mediaState = MEDIA_STATE_READY;
      safeCopy(_rt->playerStateName, sizeof(_rt->playerStateName), "READY");
      safeCopy(_rt->trackHandoffState, sizeof(_rt->trackHandoffState), "SCAN_READY");
    }

    Serial.printf("[SCANSVC] player finalized: state=%s busy=%d playReq=%d track=%s\n",
                  _playlist->size() > 0 ? "READY" : "STOPPED",
                  _rt->audioBusy ? 1 : 0, _rt->wavPlayRequest ? 1 : 0, _rt->mediaPath);

    Serial.printf("[SCANSVC] rebuild done: tracks=%d mp3=%d flac=%d wav=%d art=%d volumes=%d active=%d current=%d/%d %s\n",
                  tracks, _db->mp3(), _db->flac(), _db->wav(), _db->art(), _db->volumes(), loaded,
                  _playlist->size() ? _playlist->index() + 1 : 0, _playlist->size(), _rt->mediaPath);
    Serial.println("[SCANSVC] files: /iq200/db/media/media.idx + media_###.db + art_###.db");
    _db->printManifest();

    _library->begin();
    bool libOk = _library->buildFromMediaDb();
    _rt->libraryArtistCount = _library->artistsTotal();
    _rt->libraryAlbumCount = _library->albumsTotal();
    _rt->libraryGenreCount = _library->genresTotal();
    _rt->libraryFolderCount = _library->foldersTotal();
    _rt->libraryLastMs = _library->lastMs();
    Serial.printf("[SCANSVC][LIB] build after scan: ok=%d artists=%d albums=%d genres=%d folders=%d time=%lu ms\n",
                  libOk ? 1 : 0, _rt->libraryArtistCount, _rt->libraryAlbumCount,
                  _rt->libraryGenreCount, _rt->libraryFolderCount, (unsigned long)_rt->libraryLastMs);

    finishState("SD scan complete", true);
    post(EVT_MONITOR, _rt->dbTrackCount, _rt->dbVolumeCount, "SD scan complete");
    _task = nullptr;
  }
};
