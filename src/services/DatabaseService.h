#pragma once
#include <Arduino.h>
#include <SD.h>
#include "RuntimeState.h"
#include "MediaDatabase.h"
#include "LibraryManager.h"
#include "FavoriteManager.h"
#include "QueueManager.h"
#include "ResumeEngine.h"
#include "SDManager.h"

// IQ200 OS v8.2.5 Database Service
// Central SD database coordinator. It keeps DB families separated on SD:
//   /iq200/db/media/      media volumes + manifest
//   /iq200/db/library/    artists/albums/genres/folders/recent/most-played indexes
//   /iq200/db/favorites/  favorites DB
//   /iq200/db/queue/      queue DB
//   /iq200/db/resume/     resume state
class DatabaseService {
public:
  bool begin(RuntimeState& state,
             MediaDatabase& media,
             LibraryManager& library,
             FavoriteManager& favorites,
             QueueManager& queue,
             ResumeEngine& resume) {
    _rt = &state;
    _media = &media;
    _library = &library;
    _favorites = &favorites;
    _queue = &queue;
    _resume = &resume;
    return true;
  }

  bool startIfMounted() {
    if (_started) return true;
    if (SD.cardType() == CARD_NONE) {
      Serial.println("[DBSVC] SD not mounted; database service idle");
      return false;
    }
    ensureTree();
    migrateLegacyLayout();
    bool mediaOk = _media ? _media->begin() : false;
    bool libOk = _library ? _library->begin() : false;
    if (_favorites) _favorites->begin("/iq200/db/favorites/favorites.db");
    if (_resume && _rt) _resume->begin(*_rt);
    _started = mediaOk || libOk;
    Serial.printf("[DBSVC] start=%d media=%d library=%d paths=/iq200/db/{media,library,favorites,queue,resume}\n", _started, mediaOk, libOk);
    return _started;
  }

  void ensureTree() {
    ensureDir("/iq200");
    ensureDir("/iq200/db");
    ensureDir("/iq200/db/media");
    ensureDir("/iq200/db/library");
    ensureDir("/iq200/db/favorites");
    ensureDir("/iq200/db/queue");
    ensureDir("/iq200/db/resume");
    ensureDir("/iq200/db/art");
  }

  bool loadFavorites() { return _favorites && startIfMounted() && _favorites->load(); }
  bool saveFavorites() { SDManager::Guard g(pdMS_TO_TICKS(100)); return g && SDManager::backgroundAllowed() && _favorites && startIfMounted() && _favorites->save(); }
  bool loadQueue() { return _queue && startIfMounted() && _queue->load("/iq200/db/queue/queue.db"); }
  bool saveQueue() { SDManager::Guard g(pdMS_TO_TICKS(100)); return g && SDManager::backgroundAllowed() && _queue && startIfMounted() && _queue->save("/iq200/db/queue/queue.db"); }
  bool loadResume() { return _resume && startIfMounted() && _resume->load("/iq200/db/resume/resume.dat"); }
  bool saveResume() { if (SDManager::streamActive() || SDManager::recovering()) return false; return _resume && startIfMounted() && _resume->save("/iq200/db/resume/resume.dat"); }
  bool buildLibrary(bool force = false) { return _library && startIfMounted() && _library->buildFromMediaDb(force); }

  bool started() const { return _started; }

  void migrateLegacyLayout() {
    // v8.2.5: keep upgrades safe. Move old flat /iq200/db files into
    // separated DB families if the new target does not already exist.
    moveIfNeeded("/iq200/db/media.idx", "/iq200/db/media/media.idx");
    moveIfNeeded("/iq200/db/media.meta", "/iq200/db/media/media.meta");
    for (int i = 0; i < 999; i++) {
      char oldMedia[48], newMedia[64], oldArt[48], newArt[64];
      snprintf(oldMedia, sizeof(oldMedia), "/iq200/db/media_%03d.db", i);
      snprintf(newMedia, sizeof(newMedia), "/iq200/db/media/media_%03d.db", i);
      snprintf(oldArt, sizeof(oldArt), "/iq200/db/art_%03d.db", i);
      snprintf(newArt, sizeof(newArt), "/iq200/db/media/art_%03d.db", i);
      bool any = false;
      if (SD.exists(oldMedia)) { moveIfNeeded(oldMedia, newMedia); any = true; }
      if (SD.exists(oldArt)) { moveIfNeeded(oldArt, newArt); any = true; }
      if (!any && i > 8) break;
      delay(0);
    }
    moveIfNeeded("/iq200/db/library.meta", "/iq200/db/library/library.meta");
    moveIfNeeded("/iq200/db/artist.idx", "/iq200/db/library/artist.idx");
    moveIfNeeded("/iq200/db/album.idx", "/iq200/db/library/album.idx");
    moveIfNeeded("/iq200/db/genre.idx", "/iq200/db/library/genre.idx");
    moveIfNeeded("/iq200/db/folder.idx", "/iq200/db/library/folder.idx");
    moveIfNeeded("/iq200/db/recent.db", "/iq200/db/library/recent.db");
    moveIfNeeded("/iq200/db/mostplayed.db", "/iq200/db/library/mostplayed.db");
    moveIfNeeded("/iq200/db/favorites.db", "/iq200/db/favorites/favorites.db");
    moveIfNeeded("/iq200/db/queue.db", "/iq200/db/queue/queue.db");
    moveIfNeeded("/iq200/db/resume.dat", "/iq200/db/resume/resume.dat");
  }

  void print() const {
    Serial.println("[DBSVC] IQ200 Database Service v9.1.1");
    Serial.println("[DBSVC] Media DB     : /iq200/db/media/media.idx + media_###.db");
    Serial.println("[DBSVC] Library DB   : /iq200/db/library/{artist,album,genre,folder}.idx");
    Serial.println("[DBSVC] Favorites DB : /iq200/db/favorites/favorites.db");
    Serial.println("[DBSVC] Queue DB     : /iq200/db/queue/queue.db");
    Serial.println("[DBSVC] Resume DB    : /iq200/db/resume/resume.dat");
  }

private:
  RuntimeState* _rt = nullptr;
  MediaDatabase* _media = nullptr;
  LibraryManager* _library = nullptr;
  FavoriteManager* _favorites = nullptr;
  QueueManager* _queue = nullptr;
  ResumeEngine* _resume = nullptr;
  bool _started = false;

  static void ensureDir(const char* path) {
    if (!SD.exists(path)) SD.mkdir(path);
  }

  static void moveIfNeeded(const char* from, const char* to) {
    if (!SD.exists(from) || SD.exists(to)) return;
    if (SD.rename(from, to)) Serial.printf("[DBSVC] migrated %s -> %s\n", from, to);
  }
};
