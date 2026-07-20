#pragma once
#include <Arduino.h>
#include <SD.h>
#include "RuntimeState.h"
#include "MediaFramework.h"
#include "SDManager.h"

// IQ200 OS v8.0.2
// Small SD-backed resume state. Writes only on explicit request for now;
// future builds can call save() on track change or every 15s.
class ResumeEngine {
public:
  void begin(RuntimeState& state) { _rt = &state; }

  bool save(const char* path = "/iq200/db/resume/resume.dat") {
    if (!_rt) return false;
    SDManager::Guard guard(pdMS_TO_TICKS(100));
    if (!guard || !SDManager::backgroundAllowed()) {
      Serial.println("[SMARTRESUME] save deferred: SD stream/recovery active");
      return false;
    }
    ensureDbDir();
    File f = SD.open(path, FILE_WRITE);
    if (!f) return false;
    f.println("# IQ200 RESUME v2 TRACK_ONLY");
    f.printf("PATH=%s\n", _rt->mediaPath);
    f.printf("TITLE=%s\n", _rt->mediaTitle);
    f.printf("CODEC=%u\n", (unsigned)_rt->mediaCodec);
    // v9.1.1: resume restores only the selected track. Playback position is
    // intentionally not persisted/restored to avoid jumps after EOF, SDERR,
    // handoff, or decoder restart. Keep these keys at zero for compatibility
    // with older readers and Serial diagnostics.
    f.printf("STATE=%u\n", (unsigned)MEDIA_STATE_READY);
    f.printf("PLAYED=0\n");
    f.printf("PROGRESS=0\n");
    f.printf("VOLUME=%d\n", (int)_rt->volumePercent);
    f.printf("QUEUE_INDEX=0\n");
    f.printf("QUEUE_COUNT=0\n");
    f.printf("QUEUE_CURRENT=\n");
    f.printf("QUEUE_SHUFFLE=0\n");
    f.printf("QUEUE_REPEAT=%d\n", (int)_rt->queueRepeatMode);
    f.printf("PLAYLIST_INDEX=%d\n", (int)_rt->playlistIndex);
    f.printf("PLAYLIST_COUNT=%d\n", (int)_rt->playlistCount);
    f.printf("EQ_ENABLED=%d\n", _rt->eqEnabled ? 1 : 0);
    f.printf("EQ_BASS=%d\n", (int)_rt->eqBassDb);
    f.printf("EQ_MID=%d\n", (int)_rt->eqMidDb);
    f.printf("EQ_TREBLE=%d\n", (int)_rt->eqTrebleDb);
    f.printf("EQ_PRESET=%s\n", _rt->eqPreset);
    f.close();
    _rt->resumeLastSaveMs = millis();
    _rt->resumeSaveCount++;
    strncpy(_rt->resumeLastPath, _rt->mediaPath, sizeof(_rt->resumeLastPath)-1);
    _rt->resumeLastPath[sizeof(_rt->resumeLastPath)-1] = 0;
    return true;
  }

  bool load(const char* path = "/iq200/db/resume/resume.dat") {
    if (!_rt) return false;
    _rt->resumeLoadedPlaylistIndex = -1;
    _rt->resumeLoadedPlaylistCount = 0;
    SDManager::Guard guard(pdMS_TO_TICKS(250));
    if (!guard || !SDManager::backgroundAllowed()) return false;
    File f = SD.open(path, FILE_READ);
    if (!f) return false;
    String mediaPath;
    String title;
    uint32_t played = 0;
    uint8_t progress = 0;
    int volume = _rt->volumePercent;
    int queueIndex = 0;
    int queueCount = 0;
    int queueShuffle = 0;
    int queueRepeat = 2;
    int playlistIndex = -1;
    int playlistCount = 0;
    String queueCurrent;
    int eqEnabled = _rt->eqEnabled ? 1 : 0;
    int eqBass = _rt->eqBassDb, eqMid = _rt->eqMidDb, eqTreble = _rt->eqTrebleDb;
    String eqPreset = _rt->eqPreset;
    while (f.available()) {
      String line = f.readStringUntil('\n');
      line.trim();
      if (!line.length() || line[0] == '#') continue;
      if (line.startsWith("PATH=")) mediaPath = line.substring(5);
      else if (line.startsWith("TITLE=")) title = line.substring(6);
      else if (line.startsWith("PLAYED=")) played = (uint32_t)strtoul(line.substring(7).c_str(), nullptr, 10);
      else if (line.startsWith("PROGRESS=")) progress = (uint8_t)constrain(line.substring(9).toInt(), 0, 100);
      else if (line.startsWith("VOLUME=")) volume = constrain(line.substring(7).toInt(), 0, 100);
      else if (line.startsWith("QUEUE_INDEX=")) queueIndex = line.substring(12).toInt();
      else if (line.startsWith("QUEUE_COUNT=")) queueCount = line.substring(12).toInt();
      else if (line.startsWith("QUEUE_CURRENT=")) queueCurrent = line.substring(14);
      else if (line.startsWith("QUEUE_SHUFFLE=")) queueShuffle = line.substring(14).toInt();
      else if (line.startsWith("QUEUE_REPEAT=")) queueRepeat = line.substring(13).toInt();
      else if (line.startsWith("PLAYLIST_INDEX=")) playlistIndex = line.substring(15).toInt();
      else if (line.startsWith("PLAYLIST_COUNT=")) playlistCount = line.substring(15).toInt();
      else if (line.startsWith("EQ_ENABLED=")) eqEnabled = line.substring(11).toInt();
      else if (line.startsWith("EQ_BASS=")) eqBass = line.substring(8).toInt();
      else if (line.startsWith("EQ_MID=")) eqMid = line.substring(7).toInt();
      else if (line.startsWith("EQ_TREBLE=")) eqTreble = line.substring(10).toInt();
      else if (line.startsWith("EQ_PRESET=")) eqPreset = line.substring(10);
      delay(0);
    }
    f.close();
    if (!mediaPath.length()) return false;
    strncpy(_rt->mediaPath, mediaPath.c_str(), sizeof(_rt->mediaPath)-1);
    _rt->mediaPath[sizeof(_rt->mediaPath)-1] = 0;
    if (title.length()) {
      strncpy(_rt->mediaTitle, title.c_str(), sizeof(_rt->mediaTitle)-1);
      _rt->mediaTitle[sizeof(_rt->mediaTitle)-1] = 0;
    } else {
      const char* t = mediaTitleFromPath(_rt->mediaPath);
      strncpy(_rt->mediaTitle, t, sizeof(_rt->mediaTitle)-1);
      _rt->mediaTitle[sizeof(_rt->mediaTitle)-1] = 0;
    }
    strncpy(_rt->playlistCurrent, _rt->mediaPath, sizeof(_rt->playlistCurrent)-1);
    _rt->playlistCurrent[sizeof(_rt->playlistCurrent)-1] = 0;
    _rt->mediaCodec = mediaCodecFromPath(_rt->mediaPath);
    _rt->mediaState = MEDIA_STATE_READY;
    // v9.1.1: Track-only resume. Ignore stored PLAYED/PROGRESS even if the
    // file was written by an older build. Always start selected track from 0.
    played = 0;
    progress = 0;
    _rt->mediaPlayedBytes = 0;
    _rt->mediaProgress = 0;
    _rt->wavPlayedBytes = 0;
    _rt->wavProgress = 0;
    _rt->resumeLoadedPositionBytes = 0;
    _rt->resumeLoadedProgress = 0;
    _rt->resumeLoadedVolume = volume;
    _rt->volumePercent = volume;
    _rt->resumeLoadedPlaylistIndex = playlistIndex;
    _rt->resumeLoadedPlaylistCount = playlistCount;
    _rt->resumeLoadedQueueIndex = queueIndex;
    _rt->resumeLoadedQueueCount = queueCount;
    _rt->resumeLoadedShuffle = queueShuffle != 0;
    _rt->resumeLoadedRepeat = queueRepeat;
    _rt->queueIndex = queueIndex;
    _rt->queueCount = queueCount;
    _rt->queueShuffleSmart = queueShuffle != 0;
    _rt->queueRepeatMode = queueRepeat;
    _rt->eqEnabled = eqEnabled != 0;
    _rt->eqBassDb = constrain(eqBass, -12, 12);
    _rt->eqMidDb = constrain(eqMid, -12, 12);
    _rt->eqTrebleDb = constrain(eqTreble, -12, 12);
    strncpy(_rt->eqPreset, eqPreset.c_str(), sizeof(_rt->eqPreset)-1);
    _rt->eqPreset[sizeof(_rt->eqPreset)-1] = 0;
    if (queueCurrent.length()) {
      strncpy(_rt->queueCurrent, queueCurrent.c_str(), sizeof(_rt->queueCurrent)-1);
      _rt->queueCurrent[sizeof(_rt->queueCurrent)-1] = 0;
    }
    strncpy(_rt->resumeLastPath, _rt->mediaPath, sizeof(_rt->resumeLastPath)-1);
    _rt->resumeLastPath[sizeof(_rt->resumeLastPath)-1] = 0;
    return true;
  }

  bool clear(const char* path = "/iq200/db/resume/resume.dat") {
    if (SD.exists(path)) return SD.remove(path);
    return true;
  }

  void print(const char* path = "/iq200/db/resume/resume.dat") {
    Serial.println("[RESUME] /iq200/db/resume/resume.dat");
    File f = SD.open(path, FILE_READ);
    if (!f) { Serial.println("[RESUME] missing"); return; }
    while (f.available()) {
      String line = f.readStringUntil('\n');
      line.trim();
      if (line.length()) {
        Serial.print("[RESUME] ");
        Serial.println(line);
      }
      delay(0);
    }
    f.close();
  }

private:
  RuntimeState* _rt = nullptr;
  void ensureDbDir() {
    if (!SD.exists("/iq200")) SD.mkdir("/iq200");
    if (!SD.exists("/iq200/db")) SD.mkdir("/iq200/db");
    if (!SD.exists("/iq200/db/resume")) SD.mkdir("/iq200/db/resume");
  }
};
