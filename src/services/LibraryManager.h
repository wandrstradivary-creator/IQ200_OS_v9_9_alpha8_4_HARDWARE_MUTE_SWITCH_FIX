#pragma once
#include <Arduino.h>
#include <SD.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// IQ200 OS v8.3 MEDIA LIBRARY PRO.
// SD-backed library indexes for Artists / Albums / Genres / Folders / Recent /
// Most Played / Search. UI screens read compact index files instead of scanning
// media_###.db volumes on every open.
class LibraryManager {
public:
  static const int MAX_UNIQUE = 384;

private:
  int artistCount = 0;
  int albumCount = 0;
  int genreCount = 0;
  int folderCount = 0;
  int trackCount = 0;
  uint32_t lastBuildMs = 0;
  bool truncated = false;
  bool lastBuildSkipped = false;
  uint32_t sourceScanMs = 0;

  String artists[MAX_UNIQUE];
  String albums[MAX_UNIQUE];
  String genres[MAX_UNIQUE];
  String folders[MAX_UNIQUE];

  static void yieldSafe() { vTaskDelay(1); }

  static bool ensureDir(const char* path) {
    if (SD.exists(path)) return true;
    return SD.mkdir(path);
  }

  static String clean(String s) {
    s.trim();
    if (!s.length()) return String("Unknown");
    s.replace('_', ' ');
    s.replace('|', ' ');
    return s;
  }

  static String foldUtf8Case(String s) {
    // Arduino String::toLowerCase() only reliably handles ASCII on this target.
    // v9.1.1: add a small UTF-8 Cyrillic case fold so find "цой" matches
    // paths/titles like "Виктор Цой" even when search.idx was built earlier.
    s.toLowerCase();
    const char* upper[] = {
      "А","Б","В","Г","Д","Е","Ё","Ж","З","И","Й","К","Л","М","Н","О","П","Р","С","Т","У","Ф","Х","Ц","Ч","Ш","Щ","Ъ","Ы","Ь","Э","Ю","Я",
      "І","Ї","Є","Ґ"
    };
    const char* lower[] = {
      "а","б","в","г","д","е","ё","ж","з","и","й","к","л","м","н","о","п","р","с","т","у","ф","х","ц","ч","ш","щ","ъ","ы","ь","э","ю","я",
      "і","ї","є","ґ"
    };
    for (size_t i = 0; i < sizeof(upper) / sizeof(upper[0]); ++i) {
      s.replace(upper[i], lower[i]);
    }
    return s;
  }

  static String lowerClean(String s) {
    s = clean(s);
    return foldUtf8Case(s);
  }

  static String dirName(const String& path) {
    int slash = path.lastIndexOf('/');
    if (slash <= 0) return String("/");
    return path.substring(0, slash);
  }

  static String baseName(const String& path) {
    int slash = path.lastIndexOf('/');
    return slash >= 0 ? path.substring(slash + 1) : path;
  }

  static String nameNoExt(const String& path) {
    String b = baseName(path);
    int dot = b.lastIndexOf('.');
    if (dot > 0) b = b.substring(0, dot);
    return clean(b);
  }

  static String partAt(const String& path, int index) {
    int current = -1;
    int start = 0;
    for (int i = 0; i <= path.length(); i++) {
      if (i == path.length() || path[i] == '/') {
        if (i > start) {
          current++;
          if (current == index) return clean(path.substring(start, i));
        }
        start = i + 1;
      }
    }
    return String("Unknown");
  }

  static String parentFolder(const String& path) {
    String d = dirName(path);
    if (d == "/") return d;
    return clean(baseName(d));
  }

  static String artistFromPath(const String& path) {
    String p0 = partAt(path, 0);
    String p1 = partAt(path, 1);
    String p2 = partAt(path, 2);
    if (p0.equalsIgnoreCase("Music") || p0.equalsIgnoreCase("Музыка") || p0.equalsIgnoreCase("Muzika")) {
      return p1.length() ? p1 : String("Unknown");
    }
    if (p2 != "Unknown") return p0;
    return parentFolder(path);
  }

  static String albumFromPath(const String& path) {
    String p0 = partAt(path, 0);
    String p1 = partAt(path, 1);
    String p2 = partAt(path, 2);
    if (p0.equalsIgnoreCase("Music") || p0.equalsIgnoreCase("Музыка") || p0.equalsIgnoreCase("Muzika")) {
      return p2 != "Unknown" ? p2 : p1;
    }
    if (p2 != "Unknown") return p1;
    return parentFolder(path);
  }

  static String genreFromPath(const String& path) {
    String p0 = partAt(path, 0);
    if (p0.equalsIgnoreCase("Music") || p0.equalsIgnoreCase("Музыка") || p0.equalsIgnoreCase("Muzika")) {
      String p1 = partAt(path, 1);
      return p1.length() ? p1 : String("Unknown");
    }
    return p0.length() ? p0 : String("Unknown");
  }

  bool addUnique(String* arr, int& count, const String& value) {
    String v = clean(value);
    for (int i = 0; i < count; i++) {
      if (arr[i].equalsIgnoreCase(v)) return false;
    }
    if (count >= MAX_UNIQUE) { truncated = true; return false; }
    arr[count++] = v;
    return true;
  }

  static void writeList(const char* path, const String* arr, int count, const char* header) {
    if (SD.exists(path)) SD.remove(path);
    File f = SD.open(path, FILE_WRITE);
    if (!f) return;
    f.printf("# IQ200 %s v2 INDEXED\n", header);
    for (int i = 0; i < count; i++) {
      f.printf("%03d|%s\n", i + 1, arr[i].c_str());
      if ((i % 16) == 0) yieldSafe();
    }
    f.close();
  }

  static void printIndexedFile(const char* path, const char* label, int limit) {
    if (!SD.exists(path)) {
      Serial.printf("[%s] missing. Use libbuild after scan.\n", label);
      return;
    }
    File f = SD.open(path, FILE_READ);
    if (!f) { Serial.printf("[%s] open failed\n", label); return; }
    Serial.printf("[%s] %s\n", label, path);
    int shown = 0;
    while (f.available()) {
      String line = f.readStringUntil('\n');
      line.trim();
      if (!line.length() || line[0] == '#') { yieldSafe(); continue; }
      Serial.printf("[%s] %s\n", label, line.c_str());
      shown++;
      if (shown >= limit) { Serial.printf("[%s] ... limit %d\n", label, limit); break; }
      yieldSafe();
    }
    f.close();
  }

  bool parseMediaRecord(const String& line, String& codec, String& mediaPath) {
    if (!line.length() || line[0] == '#') return false;
    int a = line.indexOf('|');
    int b = line.indexOf('|', a + 1);
    if (a <= 0 || b <= a) return false;
    codec = line.substring(0, a);
    mediaPath = line.substring(a + 1, b);
    mediaPath.trim();
    return mediaPath.length() > 0;
  }

  static void appendLine(const char* filePath, const String& line) {
    // v9.1.1: open/append/close immediately. During burn-in scans the ESP32
    // VFS has a small file descriptor pool; keeping idx + media volume + five
    // library map files open at once caused "no free file descriptors".
    File f = SD.open(filePath, FILE_APPEND);
    if (!f) return;
    f.println(line);
    f.close();
  }

  static void appendMap(const String& codec, const String& path, const String& artist,
                        const String& album, const String& genre, const String& folder) {
    String title = nameNoExt(path);
    appendLine("/iq200/db/library/artist_tracks.idx", artist + "|" + codec + "|" + path);
    appendLine("/iq200/db/library/album_tracks.idx", album + "|" + codec + "|" + path);
    appendLine("/iq200/db/library/genre_tracks.idx", genre + "|" + codec + "|" + path);
    appendLine("/iq200/db/library/folder_tracks.idx", folder + "|" + codec + "|" + path);
    String hay = lowerClean(title + " " + artist + " " + album + " " + genre + " " + folder + " " + path);
    appendLine("/iq200/db/library/search.idx", hay + "|" + codec + "|" + title + "|" + artist + "|" + album + "|" + path);
  }

  void writeMeta() {
    if (SD.exists("/iq200/db/library/library.meta")) SD.remove("/iq200/db/library/library.meta");
    File f = SD.open("/iq200/db/library/library.meta", FILE_WRITE);
    if (!f) return;
    f.println("# IQ200 LIBRARY META v3");
    f.println("BUILD=v9.8-alpha33_SMART_LIBBUILD");
    f.println("LIB_FORMAT=3");
    f.printf("SOURCE_SCAN_MS=%lu\n", (unsigned long)sourceScanMs);
    f.printf("TRACKS=%d\n", trackCount);
    f.printf("ARTISTS=%d\n", artistCount);
    f.printf("ALBUMS=%d\n", albumCount);
    f.printf("GENRES=%d\n", genreCount);
    f.printf("FOLDERS=%d\n", folderCount);
    f.printf("TRUNCATED=%d\n", truncated ? 1 : 0);
    f.printf("LAST_BUILD_MS=%lu\n", (unsigned long)lastBuildMs);
    f.println("INDEX_MODE=SD_BACKED_PRO");
    f.println("ARTIST_INDEX=artist.idx");
    f.println("ALBUM_INDEX=album.idx");
    f.println("GENRE_INDEX=genre.idx");
    f.println("FOLDER_INDEX=folder.idx");
    f.println("SEARCH_INDEX=search.idx");
    f.println("RECENT_INDEX=recent.db");
    f.println("MOST_INDEX=mostplayed.db");
    f.close();
  }

  static bool readMetaValue(const char* path, const char* key, uint32_t& out) {
    out = 0;
    File f = SD.open(path, FILE_READ);
    if (!f) return false;
    String prefix = String(key) + "=";
    bool found = false;
    while (f.available()) {
      String line = f.readStringUntil('\n');
      line.trim();
      if (line.startsWith(prefix)) {
        out = (uint32_t)strtoul(line.substring(prefix.length()).c_str(), nullptr, 10);
        found = true;
        break;
      }
      yieldSafe();
    }
    f.close();
    return found;
  }

  static bool requiredIndexesExist() {
    const char* files[] = {
      "/iq200/db/library/library.meta",
      "/iq200/db/library/artist.idx",
      "/iq200/db/library/album.idx",
      "/iq200/db/library/genre.idx",
      "/iq200/db/library/folder.idx",
      "/iq200/db/library/artist_tracks.idx",
      "/iq200/db/library/album_tracks.idx",
      "/iq200/db/library/genre_tracks.idx",
      "/iq200/db/library/folder_tracks.idx",
      "/iq200/db/library/search.idx"
    };
    for (size_t i = 0; i < sizeof(files) / sizeof(files[0]); ++i) {
      if (!SD.exists(files[i])) return false;
    }
    return true;
  }

  bool loadMetaCounts() {
    uint32_t v = 0;
    bool ok = true;
    ok &= readMetaValue("/iq200/db/library/library.meta", "TRACKS", v); trackCount = (int)v;
    ok &= readMetaValue("/iq200/db/library/library.meta", "ARTISTS", v); artistCount = (int)v;
    ok &= readMetaValue("/iq200/db/library/library.meta", "ALBUMS", v); albumCount = (int)v;
    ok &= readMetaValue("/iq200/db/library/library.meta", "GENRES", v); genreCount = (int)v;
    ok &= readMetaValue("/iq200/db/library/library.meta", "FOLDERS", v); folderCount = (int)v;
    readMetaValue("/iq200/db/library/library.meta", "LAST_BUILD_MS", lastBuildMs);
    uint32_t trunc = 0; readMetaValue("/iq200/db/library/library.meta", "TRUNCATED", trunc); truncated = trunc != 0;
    return ok;
  }

  bool libraryIsFresh(uint32_t mediaScanMs, uint32_t mediaTracks) {
    if (!requiredIndexesExist()) return false;
    uint32_t libSourceScan = 0, libTracks = 0, formatVersion = 0;
    if (!readMetaValue("/iq200/db/library/library.meta", "SOURCE_SCAN_MS", libSourceScan)) return false;
    if (!readMetaValue("/iq200/db/library/library.meta", "TRACKS", libTracks)) return false;
    if (!readMetaValue("/iq200/db/library/library.meta", "LIB_FORMAT", formatVersion)) return false;
    return formatVersion == 3 && libSourceScan == mediaScanMs && libTracks == mediaTracks;
  }

public:
  bool begin() {
    ensureDir("/iq200");
    ensureDir("/iq200/db");
    ensureDir("/iq200/db/library");
    return true;
  }

  bool buildFromMediaDb(bool force = false) {
    begin();
    uint32_t t0 = millis();
    lastBuildSkipped = false;

    uint32_t mediaScanMs = 0;
    uint32_t mediaTracks = 0;
    const bool haveScan = readMetaValue("/iq200/db/media/media.meta", "LAST_SCAN_MS", mediaScanMs);
    const bool haveTracks = readMetaValue("/iq200/db/media/media.meta", "TRACKS", mediaTracks);
    if (!force && haveScan && haveTracks && libraryIsFresh(mediaScanMs, mediaTracks)) {
      loadMetaCounts();
      lastBuildSkipped = true;
      sourceScanMs = mediaScanMs;
      lastBuildMs = millis() - t0;
      Serial.printf("[LIBBUILD] up-to-date tracks=%lu sourceScan=%lu check=%lu ms; rebuild skipped\n",
                    (unsigned long)mediaTracks, (unsigned long)mediaScanMs, (unsigned long)lastBuildMs);
      return true;
    }
    sourceScanMs = haveScan ? mediaScanMs : 0;
    Serial.printf("[LIBBUILD] rebuild start force=%d mediaTracks=%lu sourceScan=%lu\n",
                  force ? 1 : 0, (unsigned long)mediaTracks, (unsigned long)sourceScanMs);
    artistCount = albumCount = genreCount = folderCount = trackCount = 0;
    truncated = false;
    if (!SD.exists("/iq200/db/media/media.idx")) return false;

    const char* mapFiles[] = {
      "/iq200/db/library/artist_tracks.idx",
      "/iq200/db/library/album_tracks.idx",
      "/iq200/db/library/genre_tracks.idx",
      "/iq200/db/library/folder_tracks.idx",
      "/iq200/db/library/search.idx"
    };
    for (size_t i = 0; i < sizeof(mapFiles) / sizeof(mapFiles[0]); i++) if (SD.exists(mapFiles[i])) SD.remove(mapFiles[i]);

    // Create map files and write headers one at a time to avoid exhausting VFS FDs.
    appendLine("/iq200/db/library/artist_tracks.idx", "# artist|codec|path");
    appendLine("/iq200/db/library/album_tracks.idx", "# album|codec|path");
    appendLine("/iq200/db/library/genre_tracks.idx", "# genre|codec|path");
    appendLine("/iq200/db/library/folder_tracks.idx", "# folder|codec|path");
    appendLine("/iq200/db/library/search.idx", "# haystack|codec|title|artist|album|path");

    File idx = SD.open("/iq200/db/media/media.idx", FILE_READ);
    if (!idx) return false;

    while (idx.available()) {
      String iline = idx.readStringUntil('\n');
      iline.trim();
      if (!iline.length() || iline[0] == '#') { yieldSafe(); continue; }
      if (!iline.startsWith("V|")) { yieldSafe(); continue; }

      int p1 = iline.indexOf('|');
      int p2 = iline.indexOf('|', p1 + 1);
      int p3 = iline.indexOf('|', p2 + 1);
      if (p1 < 0 || p2 < 0 || p3 < 0) { yieldSafe(); continue; }
      String mediaFile = iline.substring(p2 + 1, p3);
      File mf = SD.open(mediaFile, FILE_READ);
      if (!mf) { yieldSafe(); continue; }
      while (mf.available()) {
        String mline = mf.readStringUntil('\n');
        mline.trim();
        String codec, path;
        if (parseMediaRecord(mline, codec, path)) {
          String artist = artistFromPath(path);
          String album = albumFromPath(path);
          String genre = genreFromPath(path);
          String folder = dirName(path);
          trackCount++;
          addUnique(artists, artistCount, artist);
          addUnique(albums, albumCount, album);
          addUnique(genres, genreCount, genre);
          addUnique(folders, folderCount, folder);
          appendMap(codec, path, artist, album, genre, folder);
        }
        if ((trackCount % 16) == 0) yieldSafe();
      }
      mf.close();
      yieldSafe();
    }
    idx.close();

    writeList("/iq200/db/library/artist.idx", artists, artistCount, "ARTIST INDEX");
    writeList("/iq200/db/library/album.idx", albums, albumCount, "ALBUM INDEX");
    writeList("/iq200/db/library/genre.idx", genres, genreCount, "GENRE INDEX");
    writeList("/iq200/db/library/folder.idx", folders, folderCount, "FOLDER INDEX");
    if (!SD.exists("/iq200/db/library/recent.db")) { File r = SD.open("/iq200/db/library/recent.db", FILE_WRITE); if (r) { r.println("# IQ200 RECENT DB v2 INDEXED"); r.close(); } }
    if (!SD.exists("/iq200/db/library/mostplayed.db")) { File m = SD.open("/iq200/db/library/mostplayed.db", FILE_WRITE); if (m) { m.println("# IQ200 MOST PLAYED DB v2 INDEXED"); m.close(); } }
    lastBuildMs = millis() - t0;
    writeMeta();
    Serial.printf("[LIBBUILD] rebuild OK tracks=%d artists=%d albums=%d genres=%d folders=%d search=OK truncated=%d time=%lu ms\n",
                  trackCount, artistCount, albumCount, genreCount, folderCount, truncated ? 1 : 0,
                  (unsigned long)lastBuildMs);
    return true;
  }

  int searchIndex(const String& query, int limit = 32) {
    begin();
    String q = query;
    q.trim();
    q = foldUtf8Case(q);
    if (!q.length()) { Serial.println("[LIBSEARCH] usage: find <text>"); return 0; }
    if (!SD.exists("/iq200/db/library/search.idx")) {
      Serial.println("[LIBSEARCH] search.idx missing. Use libbuild after scan.");
      return 0;
    }
    File f = SD.open("/iq200/db/library/search.idx", FILE_READ);
    if (!f) { Serial.println("[LIBSEARCH] open failed"); return 0; }
    Serial.printf("[LIBSEARCH] query='%s' index=/iq200/db/library/search.idx\n", q.c_str());
    int shown = 0;
    int scanned = 0;
    while (f.available()) {
      String line = f.readStringUntil('\n');
      line.trim();
      if (!line.length() || line[0] == '#') { yieldSafe(); continue; }
      scanned++;
      int p1 = line.indexOf('|');
      if (p1 <= 0) continue;
      String hay = foldUtf8Case(line.substring(0, p1));
      if (hay.indexOf(q) >= 0) {
        Serial.printf("[LIBSEARCH] %03d %s\n", shown + 1, line.substring(p1 + 1).c_str());
        shown++;
        if (shown >= limit) { Serial.printf("[LIBSEARCH] ... limit %d scanned=%d\n", limit, scanned); break; }
      }
      if ((scanned % 16) == 0) yieldSafe();
    }
    f.close();
    Serial.printf("[LIBSEARCH] results=%d scanned=%d\n", shown, scanned);
    return shown;
  }

  void printTracksForKey(const char* mapPath, const char* label, const String& key, int limit = 32) {
    begin();
    String k = clean(key);
    if (!SD.exists(mapPath)) { Serial.printf("[%s] track map missing. Use libbuild.\n", label); return; }
    File f = SD.open(mapPath, FILE_READ);
    if (!f) { Serial.printf("[%s] open failed\n", label); return; }
    Serial.printf("[%s] tracks for '%s'\n", label, k.c_str());
    int shown = 0;
    while (f.available()) {
      String line = f.readStringUntil('\n');
      line.trim();
      if (!line.length() || line[0] == '#') { yieldSafe(); continue; }
      int p = line.indexOf('|');
      if (p <= 0) continue;
      String lhs = line.substring(0, p);
      if (lhs.equalsIgnoreCase(k)) {
        Serial.printf("[%s] %03d %s\n", label, shown + 1, line.substring(p + 1).c_str());
        shown++;
        if (shown >= limit) { Serial.printf("[%s] ... limit %d\n", label, limit); break; }
      }
      if ((shown % 8) == 0) yieldSafe();
    }
    f.close();
    Serial.printf("[%s] shown=%d\n", label, shown);
  }

  void printList(const char* path, const char* label, int limit = 24) {
    begin();
    printIndexedFile(path, label, limit);
  }

  void printStats() {
    begin();
    if (SD.exists("/iq200/db/library/library.meta")) {
      File f = SD.open("/iq200/db/library/library.meta", FILE_READ);
      if (f) {
        Serial.println("[LIB] /iq200/db/library/library.meta");
        while (f.available()) {
          String line = f.readStringUntil('\n');
          line.trim();
          if (line.length()) Serial.println(String("[LIB] ") + line);
          yieldSafe();
        }
        f.close();
      }
    } else {
      Serial.println("[LIB] library.meta missing. Use libbuild.");
    }
  }

  int artistsTotal() const { return artistCount; }
  int albumsTotal() const { return albumCount; }
  int genresTotal() const { return genreCount; }
  int foldersTotal() const { return folderCount; }
  int tracksTotal() const { return trackCount; }
  uint32_t lastMs() const { return lastBuildMs; }
  bool wasTruncated() const { return truncated; }
  bool buildWasSkipped() const { return lastBuildSkipped; }
  uint32_t sourceScanTimestamp() const { return sourceScanMs; }
};
