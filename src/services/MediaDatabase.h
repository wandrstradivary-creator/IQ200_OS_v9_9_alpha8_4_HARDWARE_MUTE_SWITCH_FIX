#pragma once
#include <Arduino.h>
#include <SD.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "PlaylistManager.h"
#include "MediaFramework.h"

class MediaDatabase {
public:
  static const int DB_LINES_PER_VOLUME = 512;
  static const int DB_MAX_DEPTH = 12;
  static const int DB_YIELD_EVERY = 6;
  static const int DB_MAX_IGNORE_RULES = 48;
  static const int DB_MAX_VOLUMES = 999;

private:
  int totalTracks = 0;
  int totalMp3 = 0;
  int totalFlac = 0;
  int totalWav = 0;
  int totalArt = 0;
  int totalFiles = 0;
  int volumeIndex = 0;
  int volumeLineCount = 0;
  int volumeMediaLines[DB_MAX_VOLUMES];
  int volumeArtLines[DB_MAX_VOLUMES];
  uint32_t visited = 0;
  String ignoreRules[DB_MAX_IGNORE_RULES];
  int ignoreRuleCount = 0;
  bool ignoreRulesLoaded = false;
  File mediaVol;
  File artVol;
  volatile int* progressPtr = nullptr;
  volatile int* filesPtr = nullptr;
  volatile int* tracksPtr = nullptr;
  volatile int* dirsPtr = nullptr;
  volatile int* mp3Ptr = nullptr;
  volatile int* flacPtr = nullptr;
  volatile int* wavPtr = nullptr;
  char* messagePtr = nullptr;
  size_t messageLen = 0;
  char* currentPathPtr = nullptr;
  size_t currentPathLen = 0;

  static bool ensureDir(const char* path) {
    if (SD.exists(path)) return true;
    return SD.mkdir(path);
  }

  static String volumePath(const char* prefix, int idx) {
    char buf[48];
    snprintf(buf, sizeof(buf), "/iq200/db/media/%s_%03d.db", prefix, idx);
    return String(buf);
  }

  static const char* metaPath() {
    return "/iq200/db/media/media.meta";
  }

  static const char* indexPath() {
    return "/iq200/db/media/media.idx";
  }

  static String normalizeVolumePath(String p) {
    // v8.2.6: old media.idx files may still reference the legacy flat layout
    // /iq200/db/media_000.db after DatabaseService migrated files to
    // /iq200/db/media/media_000.db. Normalize before SD.open() to avoid
    // harmless but noisy VFS errors and empty boot playlists.
    if (p.startsWith("/iq200/db/media_") && p.endsWith(".db")) {
      String name = p.substring(String("/iq200/db/").length());
      return String("/iq200/db/media/") + name;
    }
    if (p.startsWith("/iq200/db/art_") && p.endsWith(".db")) {
      String name = p.substring(String("/iq200/db/").length());
      return String("/iq200/db/media/") + name;
    }
    return p;
  }

  static String dirName(const String& path) {
    int slash = path.lastIndexOf('/');
    if (slash <= 0) return String("/");
    return path.substring(0, slash);
  }

  static String baseNameNoExt(const String& path) {
    int slash = path.lastIndexOf('/');
    int dot = path.lastIndexOf('.');
    int start = slash >= 0 ? slash + 1 : 0;
    if (dot <= start) dot = path.length();
    return path.substring(start, dot);
  }

  static String joinPath(const String& dir, const String& name) {
    if (dir == "/") return String("/") + name;
    return dir + "/" + name;
  }

  static String normalizePath(String p) {
    p.replace('\\', '/');
    p.toLowerCase();
    while (p.indexOf("//") >= 0) p.replace("//", "/");
    return p;
  }

  static bool ruleMatchesPath(const String& pathLower, String rule) {
    rule.trim();
    if (!rule.length() || rule[0] == '#') return false;
    rule = normalizePath(rule);
    if (!rule.length()) return false;

    // Hidden/system folders: rule like ".trash" should match any path segment.
    if (pathLower == rule) return true;
    if (pathLower.endsWith("/" + rule)) return true;
    if (pathLower.indexOf("/" + rule + "/") >= 0) return true;

    // Subtree rule: "Android/data" should match /Android/data/... exactly as a branch.
    if (rule.indexOf('/') >= 0) {
      if (pathLower.indexOf("/" + rule) >= 0) return true;
      if (pathLower.startsWith(rule)) return true;
    }
    return false;
  }

  void addIgnoreRule(const String& rule) {
    if (ignoreRuleCount >= DB_MAX_IGNORE_RULES) return;
    String r = rule;
    r.trim();
    if (!r.length() || r[0] == '#') return;
    ignoreRules[ignoreRuleCount++] = r;
  }

  void writeDefaultIgnoreConfig() {
    ensureDir("/iq200");
    ensureDir("/iq200/config");
    if (SD.exists("/iq200/config/scan_ignore.txt")) return;
    File f = SD.open("/iq200/config/scan_ignore.txt", FILE_WRITE);
    if (!f) return;
    f.println("# IQ200 scan ignore list");
    f.println("System Volume Information");
    f.println("$RECYCLE.BIN");
    f.println("LOST.DIR");
    f.println(".Trash");
    f.println(".Trashes");
    f.println(".fseventsd");
    f.println(".Spotlight-V100");
    f.println("Android/data");
    f.println("Android/obb");
    f.println("DCIM/.thumbnails");
    f.println("cache");
    f.println("tmp");
    f.println("Temp");
    f.close();
  }

  void loadIgnoreRules() {
    if (ignoreRulesLoaded) return;
    ignoreRuleCount = 0;
    writeDefaultIgnoreConfig();

    File f = SD.open("/iq200/config/scan_ignore.txt", FILE_READ);
    if (f) {
      while (f.available() && ignoreRuleCount < DB_MAX_IGNORE_RULES) {
        String line = f.readStringUntil('\n');
        line.trim();
        addIgnoreRule(line);
        scannerYield();
      }
      f.close();
    }

    // Hard safety fallback if config is missing or empty.
    if (ignoreRuleCount == 0) {
      addIgnoreRule("System Volume Information");
      addIgnoreRule("$RECYCLE.BIN");
      addIgnoreRule("LOST.DIR");
      addIgnoreRule(".Trash");
      addIgnoreRule(".Trashes");
      addIgnoreRule(".fseventsd");
      addIgnoreRule(".Spotlight-V100");
      addIgnoreRule("Android/data");
      addIgnoreRule("Android/obb");
      addIgnoreRule("DCIM/.thumbnails");
      addIgnoreRule("cache");
      addIgnoreRule("tmp");
      addIgnoreRule("Temp");
    }
    ignoreRulesLoaded = true;
  }

  bool isSkipDir(const String& path) {
    loadIgnoreRules();
    String p = normalizePath(path);
    for (int i = 0; i < ignoreRuleCount; i++) {
      if (ruleMatchesPath(p, ignoreRules[i])) return true;
    }
    return false;
  }

  static bool hasExt(const String& path, const char* ext) {
    String p = path;
    p.toLowerCase();
    return p.endsWith(ext);
  }

  static const char* codecForPath(const String& path) {
    if (hasExt(path, ".flac")) return "FLAC";
    if (hasExt(path, ".opus")) return "OPUS";
    if (hasExt(path, ".mp3")) return "MP3";
    if (hasExt(path, ".ogg") || hasExt(path, ".oga")) return "OGG";
    if (hasExt(path, ".wav")) return "WAV";
    if (hasExt(path, ".aac") || hasExt(path, ".m4a")) return "AAC";
    return "";
  }

  static int codecPriority(const String& path) {
    if (hasExt(path, ".flac")) return 0;
    if (hasExt(path, ".opus")) return 1;
    if (hasExt(path, ".mp3")) return 2;
    if (hasExt(path, ".ogg") || hasExt(path, ".oga")) return 3;
    if (hasExt(path, ".wav")) return 4;
    if (hasExt(path, ".aac") || hasExt(path, ".m4a")) return 5;
    return 9;
  }

  static bool isMedia(const String& path) {
    return codecPriority(path) <= 5;
  }

  static bool isArt(const String& path) {
    String p = path;
    p.toLowerCase();
    return p.endsWith(".bmp") || p.endsWith(".jpg") || p.endsWith(".jpeg");
  }

  void scannerYield() {
    // v7.5.1: scannerYield is only for WDT/FreeRTOS cooperation.
    // File count is now a real file counter, not a yield counter.
    visited++;
    if ((visited % DB_YIELD_EVERY) == 0) vTaskDelay(1);
  }

  void setMessage(const char* msg) {
    if (!messagePtr || !messageLen) return;
    strncpy(messagePtr, msg, messageLen - 1);
    messagePtr[messageLen - 1] = 0;
  }

  void setProgress(int p) {
    if (p < 0) p = 0;
    if (p > 100) p = 100;
    if (progressPtr) *progressPtr = p;
  }

  void countFile() {
    totalFiles++;
    if (filesPtr) *filesPtr = totalFiles;
  }

  void setCurrentPath(const String& path) {
    if (!currentPathPtr || !currentPathLen) return;
    strncpy(currentPathPtr, path.c_str(), currentPathLen - 1);
    currentPathPtr[currentPathLen - 1] = 0;
  }

  void updateCodecCounters() {
    if (tracksPtr) *tracksPtr = totalTracks;
    if (mp3Ptr) *mp3Ptr = totalMp3;
    if (flacPtr) *flacPtr = totalFlac;
    if (wavPtr) *wavPtr = totalWav;
  }

  bool openVolumesIfNeeded() {
    if (mediaVol && artVol) return true;
    ensureDir("/iq200");
    ensureDir("/iq200/db");
    ensureDir("/iq200/db/media");
    ensureDir("/iq200/db/art");
    ensureDir("/iq200/art");
    volumeIndex = 0;
    volumeLineCount = 0;
    mediaVol = SD.open(volumePath("media", volumeIndex), FILE_WRITE);
    artVol = SD.open(volumePath("art", volumeIndex), FILE_WRITE);
    if (!mediaVol || !artVol) return false;
    mediaVol.println("# IQ200 MEDIA DB v1 MULTIVOLUME");
    artVol.println("# IQ200 ART DB v1 MULTIVOLUME");
    return true;
  }

  void rotateVolumeIfNeeded() {
    if (volumeLineCount < DB_LINES_PER_VOLUME) return;
    if (mediaVol) mediaVol.close();
    if (artVol) artVol.close();
    volumeIndex++;
    volumeLineCount = 0;
    mediaVol = SD.open(volumePath("media", volumeIndex), FILE_WRITE);
    artVol = SD.open(volumePath("art", volumeIndex), FILE_WRITE);
    if (mediaVol) mediaVol.println("# IQ200 MEDIA DB v1 MULTIVOLUME");
    if (artVol) artVol.println("# IQ200 ART DB v1 MULTIVOLUME");
    scannerYield();
  }

  String findAlbumArt(const String& mediaPath) {
    String d = dirName(mediaPath);
    const char* names[] = {
      "cover.bmp", "folder.bmp", "album.bmp",
      "cover.jpg", "folder.jpg", "album.jpg",
      "cover.jpeg", "folder.jpeg", "album.jpeg"
    };
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
      String candidate = joinPath(d, names[i]);
      if (SD.exists(candidate)) return candidate;
      scannerYield();
    }
    String byTrackBmp = joinPath(d, baseNameNoExt(mediaPath) + ".bmp");
    if (SD.exists(byTrackBmp)) return byTrackBmp;
    String byTrackJpg = joinPath(d, baseNameNoExt(mediaPath) + ".jpg");
    if (SD.exists(byTrackJpg)) return byTrackJpg;
    return String("CD_DISK_INTERNAL");
  }

  void writeMediaEntry(const String& path) {
    // v7.5: single-pass scanner. Do not call SD.exists/stat for album art per track;
    // this keeps SD traversal fast. Album art files are discovered in the same pass
    // and written to art_###.db independently. Player fallback remains CD_DISK_INTERNAL.
    if (!openVolumesIfNeeded()) return;
    rotateVolumeIfNeeded();
    const char* codec = codecForPath(path);
    mediaVol.printf("%s|%s|CD_DISK_INTERNAL\n", codec, path.c_str());
    totalTracks++;
    if (volumeIndex >= 0 && volumeIndex < DB_MAX_VOLUMES) volumeMediaLines[volumeIndex]++;
    if (strcmp(codec, "MP3") == 0) totalMp3++;
    else if (strcmp(codec, "FLAC") == 0) totalFlac++;
    else if (strcmp(codec, "WAV") == 0) totalWav++;
    updateCodecCounters();
    volumeLineCount++;
    scannerYield();
  }

  void writeArtFileEntry(const String& path) {
    // Store discovered cover candidates. Later versions can resolve best cover per album
    // from this SD-backed art database without rescanning every media file.
    if (!openVolumesIfNeeded()) return;
    rotateVolumeIfNeeded();
    artVol.printf("ART|%s\n", path.c_str());
    totalArt++;
    if (volumeIndex >= 0 && volumeIndex < DB_MAX_VOLUMES) volumeArtLines[volumeIndex]++;
    volumeLineCount++;
    scannerYield();
  }

  void scanDirUnified(const String& dirPath, int depth) {
    if (depth > DB_MAX_DEPTH) return;
    if (isSkipDir(dirPath)) return;

    if (dirsPtr) (*dirsPtr)++;
    setCurrentPath(dirPath);

    File dir = SD.open(dirPath);
    if (!dir) {
      scannerYield();
      return;
    }

    while (true) {
      File f = dir.openNextFile();
      if (!f) break;
      String name = String(f.name());
      String path = joinPath(dirPath, name);
      bool isDir = f.isDirectory();
      f.close();

      if (isDir) {
        if (!isSkipDir(path)) scanDirUnified(path, depth + 1);
      } else {
        countFile();
        if (isMedia(path)) {
          writeMediaEntry(path);
        } else if (isArt(path)) {
          writeArtFileEntry(path);
        }
      }
      scannerYield();
    }

    dir.close();
    scannerYield();
  }

public:
  void attachProgress(volatile int* progress, volatile int* files, volatile int* tracks, char* message, size_t msgLen,
                      volatile int* dirs = nullptr, volatile int* mp3 = nullptr, volatile int* flac = nullptr, volatile int* wav = nullptr,
                      char* currentPath = nullptr, size_t currentLen = 0) {
    progressPtr = progress;
    filesPtr = files;
    tracksPtr = tracks;
    messagePtr = message;
    messageLen = msgLen;
    dirsPtr = dirs;
    mp3Ptr = mp3;
    flacPtr = flac;
    wavPtr = wav;
    currentPathPtr = currentPath;
    currentPathLen = currentLen;
  }

  bool begin() {
    // v8.2.1 Build Verify: caller must mount SD first. Avoid noisy VFS
    // "File system is not mounted" errors during early boot.
    if (SD.cardType() == CARD_NONE) {
      Serial.println("[DB] begin skipped: SD not mounted");
      return false;
    }
    ensureDir("/iq200");
    ensureDir("/iq200/db");
    ensureDir("/iq200/db/media");
    ensureDir("/iq200/db/art");
    ensureDir("/iq200/art");
    ensureDir("/iq200/config");
    loadIgnoreRules();
    return true;
  }

  void clearDbFiles() {
    if (SD.exists(metaPath())) SD.remove(metaPath());
    if (SD.exists(indexPath())) SD.remove(indexPath());
    for (int i = 0; i < 999; i++) {
      String m = volumePath("media", i);
      String a = volumePath("art", i);
      bool any = false;
      if (SD.exists(m)) { SD.remove(m); any = true; }
      if (SD.exists(a)) { SD.remove(a); any = true; }
      if (!any && i > 8) break;
      scannerYield();
    }
  }

  int rebuild() {
    totalTracks = totalMp3 = totalFlac = totalWav = totalArt = 0;
    totalFiles = 0;
    for (int i = 0; i < DB_MAX_VOLUMES; i++) {
      volumeMediaLines[i] = 0;
      volumeArtLines[i] = 0;
    }
    visited = 0;
    if (dirsPtr) *dirsPtr = 0;
    if (filesPtr) *filesPtr = 0;
    updateCodecCounters();
    setCurrentPath("/");
    ignoreRulesLoaded = false;
    setProgress(1);
    setMessage("Preparing SD database...");
    if (mediaVol) mediaVol.close();
    if (artVol) artVol.close();
    clearDbFiles();
    openVolumesIfNeeded();

    // v7.5.1: one SD traversal classifies all supported formats at the same time:
    // MP3 / FLAC / WAV / album art. This avoids three full passes over FAT/SD.
    setProgress(10);
    setMessage("Scanning all media formats...");
    scanDirUnified("/", 0);
    setProgress(95);
    setMessage("Writing database...");

    if (mediaVol) mediaVol.close();
    if (artVol) artVol.close();
    writeIndex();
    writeManifest();
    setProgress(100);
    setMessage("Scan complete");
    return totalTracks;
  }



  bool writeIndex() {
    ensureDir("/iq200");
    ensureDir("/iq200/db");
    File f = SD.open(indexPath(), FILE_WRITE);
    if (!f) return false;
    f.println("# IQ200 MEDIA INDEX v1");
    f.println("# V|index|media_path|media_records|art_path|art_records");
    for (int i = 0; i <= volumeIndex && i < DB_MAX_VOLUMES; i++) {
      String mp = volumePath("media", i);
      String ap = volumePath("art", i);
      f.printf("V|%03d|%s|%d|%s|%d\n",
               i, mp.c_str(), volumeMediaLines[i], ap.c_str(), volumeArtLines[i]);
      scannerYield();
    }
    f.close();
    return true;
  }

  bool writeManifest() {
    ensureDir("/iq200");
    ensureDir("/iq200/db");
    File f = SD.open(metaPath(), FILE_WRITE);
    if (!f) return false;
    f.println("# IQ200 MEDIA META v1");
    f.println("DB_VERSION=1");
    f.println("BUILD=v9.1.1_NEXT_TRACK_ENGINE");
    f.printf("TRACKS=%d\n", totalTracks);
    f.printf("MP3=%d\n", totalMp3);
    f.printf("FLAC=%d\n", totalFlac);
    f.printf("WAV=%d\n", totalWav);
    f.printf("ART=%d\n", totalArt);
    f.printf("FILES=%d\n", totalFiles);
    f.printf("VOLUMES=%d\n", volumeIndex + 1);
    f.printf("LINES_PER_VOLUME=%d\n", DB_LINES_PER_VOLUME);
    f.printf("LAST_SCAN_MS=%lu\n", (unsigned long)millis());
    f.println("MODE=SINGLE_PASS_INDEXED");
    f.println("INDEX=media.idx");
    f.close();
    scannerYield();
    return true;
  }

  bool readManifest(int* tracksOut = nullptr, int* volumesOut = nullptr, int* artOut = nullptr,
                    int* mp3Out = nullptr, int* flacOut = nullptr, int* wavOut = nullptr, int* filesOut = nullptr) {
    if (!SD.exists(metaPath())) return false;
    File f = SD.open(metaPath(), FILE_READ);
    if (!f) return false;
    int tracks = -1, volumes = -1, art = -1, mp3 = -1, flac = -1, wav = -1, files = -1;
    while (f.available()) {
      String line = f.readStringUntil('\n');
      line.trim();
      if (!line.length() || line[0] == '#') { scannerYield(); continue; }
      int eq = line.indexOf('=');
      if (eq <= 0) { scannerYield(); continue; }
      String key = line.substring(0, eq);
      String val = line.substring(eq + 1);
      key.trim(); val.trim();
      if (key == "TRACKS") tracks = val.toInt();
      else if (key == "VOLUMES") volumes = val.toInt();
      else if (key == "ART") art = val.toInt();
      else if (key == "MP3") mp3 = val.toInt();
      else if (key == "FLAC") flac = val.toInt();
      else if (key == "WAV") wav = val.toInt();
      else if (key == "FILES") files = val.toInt();
      scannerYield();
    }
    f.close();
    if (tracksOut && tracks >= 0) *tracksOut = tracks;
    if (volumesOut && volumes >= 0) *volumesOut = volumes;
    if (artOut && art >= 0) *artOut = art;
    if (mp3Out && mp3 >= 0) *mp3Out = mp3;
    if (flacOut && flac >= 0) *flacOut = flac;
    if (wavOut && wav >= 0) *wavOut = wav;
    if (filesOut && files >= 0) *filesOut = files;
    return tracks >= 0;
  }

  bool validateManifestFast(bool verbose = true) {
    int tracks = -1, volumes = -1, art = -1, mp3 = -1, flac = -1, wav = -1, files = -1;
    if (!readManifest(&tracks, &volumes, &art, &mp3, &flac, &wav, &files)) {
      if (verbose) Serial.println("[DBFAST] media.meta missing or invalid");
      return false;
    }
    if (!SD.exists(indexPath())) {
      if (verbose) Serial.println("[DBFAST] media.idx missing");
      return false;
    }
    if (tracks < 0 || volumes <= 0) {
      if (verbose) Serial.printf("[DBFAST] invalid counters: tracks=%d volumes=%d\n", tracks, volumes);
      return false;
    }
    for (int i = 0; i < volumes; i++) {
      String m = volumePath("media", i);
      if (!SD.exists(m)) {
        if (verbose) Serial.printf("[DBFAST] missing volume: %s\n", m.c_str());
        return false;
      }
      scannerYield();
    }
    totalTracks = tracks;
    totalMp3 = mp3 >= 0 ? mp3 : 0;
    totalFlac = flac >= 0 ? flac : 0;
    totalWav = wav >= 0 ? wav : 0;
    totalArt = art >= 0 ? art : 0;
    totalFiles = files >= 0 ? files : 0;
    volumeIndex = volumes > 0 ? volumes - 1 : 0;
    if (verbose) {
      Serial.printf("[DBFAST] OK tracks=%d mp3=%d flac=%d wav=%d art=%d volumes=%d\n",
                    totalTracks, totalMp3, totalFlac, totalWav, totalArt, volumes);
    }
    return true;
  }

  void printManifest() {
    if (!SD.exists(metaPath())) {
      Serial.println("[DBMETA] media.meta missing");
      return;
    }
    File f = SD.open(metaPath(), FILE_READ);
    if (!f) {
      Serial.println("[DBMETA] open failed");
      return;
    }
    Serial.println("[DBMETA] /iq200/db/media/media.meta");
    while (f.available()) {
      String line = f.readStringUntil('\n');
      line.trim();
      if (line.length()) Serial.println(String("[DBMETA] ") + line);
      scannerYield();
    }
    f.close();
  }

  void printIndex() {
    if (!SD.exists(indexPath())) {
      Serial.println("[DBIDX] media.idx missing");
      return;
    }
    File f = SD.open(indexPath(), FILE_READ);
    if (!f) {
      Serial.println("[DBIDX] open failed");
      return;
    }
    Serial.println("[DBIDX] /iq200/db/media/media.idx");
    int shown = 0;
    while (f.available()) {
      String line = f.readStringUntil('\n');
      line.trim();
      if (line.length()) {
        Serial.println(String("[DBIDX] ") + line);
        shown++;
      }
      if (shown >= 24) {
        Serial.println("[DBIDX] ...");
        break;
      }
      scannerYield();
    }
    f.close();
  }

  int loadPlaylist(PlaylistManager& pl) {
    pl.clear();
    int loaded = 0;

    // v7.7: use media.idx when present so boot does not search for every possible volume.
    if (SD.exists(indexPath())) {
      File idx = SD.open(indexPath(), FILE_READ);
      if (idx) {
        while (idx.available()) {
          String line = idx.readStringUntil('\n');
          line.trim();
          if (!line.length() || line[0] == '#') { scannerYield(); continue; }
          if (!line.startsWith("V|")) { scannerYield(); continue; }
          int p1 = line.indexOf('|');
          int p2 = line.indexOf('|', p1 + 1);
          int p3 = line.indexOf('|', p2 + 1);
          int p4 = line.indexOf('|', p3 + 1);
          int p5 = line.indexOf('|', p4 + 1);
          if (p1 < 0 || p2 < 0 || p3 < 0 || p4 < 0 || p5 < 0) { scannerYield(); continue; }
          String mediaPath = normalizeVolumePath(line.substring(p2 + 1, p3));
          int recs = line.substring(p3 + 1, p4).toInt();
          if (recs <= 0) { scannerYield(); continue; }
          File f = SD.open(mediaPath, FILE_READ);
          if (!f) { scannerYield(); continue; }
          while (f.available()) {
            String mline = f.readStringUntil('\n');
            mline.trim();
            if (!mline.length() || mline[0] == '#') { scannerYield(); continue; }
            int q1 = mline.indexOf('|');
            int q2 = mline.indexOf('|', q1 + 1);
            if (q1 > 0 && q2 > q1) {
              String path = mline.substring(q1 + 1, q2);
              if (pl.add(path)) loaded++;
            }
            scannerYield();
          }
          f.close();
          scannerYield();
        }
        idx.close();
        return loaded;
      }
    }

    // Compatibility fallback for old databases without media.idx.
    for (int v = 0; v < 999; v++) {
      String p = volumePath("media", v);
      if (!SD.exists(p)) {
        if (v > 0) break;
        continue;
      }
      File f = SD.open(p, FILE_READ);
      if (!f) continue;
      while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (!line.length() || line[0] == '#') continue;
        int p1 = line.indexOf('|');
        int p2 = line.indexOf('|', p1 + 1);
        if (p1 > 0 && p2 > p1) {
          String path = line.substring(p1 + 1, p2);
          if (pl.add(path)) loaded++;
        }
        scannerYield();
      }
      f.close();
    }
    return loaded;
  }


  int find(const String& query, int maxResults = 24) {
    String q = query;
    q.trim();
    q.toLowerCase();
    if (!q.length()) {
      Serial.println("[FIND] usage: find <text>");
      return 0;
    }
    if (!SD.exists(indexPath())) {
      Serial.println("[FIND] media.idx missing. Run scan/rescan first.");
      return 0;
    }

    int shown = 0;
    int scanned = 0;
    Serial.printf("[FIND] searching: %s limit=%d\n", q.c_str(), maxResults);

    File idx = SD.open(indexPath(), FILE_READ);
    if (!idx) {
      Serial.println("[FIND] open media.idx failed");
      return 0;
    }

    while (idx.available()) {
      String line = idx.readStringUntil('\n');
      line.trim();
      if (!line.length() || line[0] == '#') { scannerYield(); continue; }
      if (!line.startsWith("V|")) { scannerYield(); continue; }

      int p1 = line.indexOf('|');
      int p2 = line.indexOf('|', p1 + 1);
      int p3 = line.indexOf('|', p2 + 1);
      if (p1 < 0 || p2 < 0 || p3 < 0) { scannerYield(); continue; }
      String mediaPath = normalizeVolumePath(line.substring(p2 + 1, p3));

      File f = SD.open(mediaPath, FILE_READ);
      if (!f) { scannerYield(); continue; }
      while (f.available()) {
        String mline = f.readStringUntil('\n');
        mline.trim();
        if (!mline.length() || mline[0] == '#') { scannerYield(); continue; }
        scanned++;

        int a = mline.indexOf('|');
        int b = mline.indexOf('|', a + 1);
        if (a <= 0 || b <= a) { scannerYield(); continue; }
        String codec = mline.substring(0, a);
        String path = mline.substring(a + 1, b);
        String low = path;
        low.toLowerCase();
        if (low.indexOf(q) >= 0) {
          shown++;
          String title = path;
          int slash = title.lastIndexOf('/');
          if (slash >= 0) title = title.substring(slash + 1);
          int dot = title.lastIndexOf('.');
          if (dot > 0) title = title.substring(0, dot);
          if (title.length() > 54) title = title.substring(0, 51) + "...";
          Serial.printf("> %02d [%s] %s\n", shown, codec.c_str(), title.c_str());
          Serial.printf("     %s\n", path.c_str());
          if (shown >= maxResults) {
            Serial.printf("[FIND] limit %d reached; scanned=%d. Use findall <text> for more.\n", maxResults, scanned);
            f.close();
            idx.close();
            return shown;
          }
        }
        scannerYield();
      }
      f.close();
      scannerYield();
    }
    idx.close();
    Serial.printf("[FIND] done results=%d scanned=%d\n", shown, scanned);
    return shown;
  }

  int selfTest(bool verbose = true) {
    int errors = 0;
    int mediaVolumes = 0;
    int artVolumes = 0;
    int mediaLines = 0;
    int artLines = 0;

    if (verbose) Serial.println("[DBTEST] begin");

    if (!SD.exists("/iq200/db")) {
      if (verbose) Serial.println("[DBTEST] ERROR: /iq200/db missing");
      return 1;
    }
    if (!SD.exists(indexPath())) {
      errors++;
      if (verbose) Serial.println("[DBTEST] ERROR: media.idx missing");
    }

    for (int v = 0; v < 999; v++) {
      String mp = volumePath("media", v);
      String ap = volumePath("art", v);
      bool mediaExists = SD.exists(mp);
      bool artExists = SD.exists(ap);

      if (!mediaExists && !artExists) {
        if (v > 0) break;
        continue;
      }

      if (mediaExists) {
        mediaVolumes++;
        File f = SD.open(mp, FILE_READ);
        if (!f) {
          errors++;
          if (verbose) Serial.printf("[DBTEST] ERROR open %s\n", mp.c_str());
        } else {
          int lineNo = 0;
          while (f.available()) {
            String line = f.readStringUntil('\n');
            line.trim();
            lineNo++;
            if (!line.length() || line[0] == '#') { scannerYield(); continue; }
            int p1 = line.indexOf('|');
            int p2 = line.indexOf('|', p1 + 1);
            if (p1 <= 0 || p2 <= p1) {
              errors++;
              if (verbose) Serial.printf("[DBTEST] ERROR media format %s:%d\n", mp.c_str(), lineNo);
            } else {
              String codec = line.substring(0, p1);
              String path = line.substring(p1 + 1, p2);
              if (!(codec == "MP3" || codec == "FLAC" || codec == "WAV")) {
                errors++;
                if (verbose) Serial.printf("[DBTEST] ERROR codec %s:%d %s\n", mp.c_str(), lineNo, codec.c_str());
              }
              if (!path.startsWith("/")) {
                errors++;
                if (verbose) Serial.printf("[DBTEST] ERROR path %s:%d %s\n", mp.c_str(), lineNo, path.c_str());
              }
              mediaLines++;
            }
            scannerYield();
          }
          f.close();
        }
      }

      if (artExists) {
        artVolumes++;
        File f = SD.open(ap, FILE_READ);
        if (!f) {
          errors++;
          if (verbose) Serial.printf("[DBTEST] ERROR open %s\n", ap.c_str());
        } else {
          int lineNo = 0;
          while (f.available()) {
            String line = f.readStringUntil('\n');
            line.trim();
            lineNo++;
            if (!line.length() || line[0] == '#') { scannerYield(); continue; }
            int p1 = line.indexOf('|');
            if (p1 <= 0) {
              errors++;
              if (verbose) Serial.printf("[DBTEST] ERROR art format %s:%d\n", ap.c_str(), lineNo);
            } else {
              artLines++;
            }
            scannerYield();
          }
          f.close();
        }
      }
      scannerYield();
    }

    if (mediaVolumes == 0) {
      errors++;
      if (verbose) Serial.println("[DBTEST] ERROR: no media volumes");
    }

    if (verbose) {
      Serial.printf("[DBTEST] mediaVolumes=%d artVolumes=%d mediaLines=%d artLines=%d errors=%d\n",
                    mediaVolumes, artVolumes, mediaLines, artLines, errors);
      Serial.println(errors == 0 ? "[DBTEST] PASS" : "[DBTEST] FAIL");
    }
    return errors;
  }

  bool exists() const {
    return SD.exists("/iq200/db/media/media_000.db");
  }

  int tracks() const { return totalTracks; }
  int mp3() const { return totalMp3; }
  int flac() const { return totalFlac; }
  int wav() const { return totalWav; }
  int art() const { return totalArt; }
  int files() const { return totalFiles; }
  int volumes() const { return volumeIndex + 1; }
};
