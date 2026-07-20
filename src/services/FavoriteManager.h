#pragma once
#include <Arduino.h>
#include <SD.h>

// IQ200 OS v8.1.5
// SD-backed Favorites foundation. Header-only to keep PlatformIO integration simple.
class FavoriteManager {
public:
  static const int MAX_FAVORITES = 128;

  void begin(const char* path = "/iq200/db/favorites/favorites.db") {
    _path = path;
  }

  bool add(const String& path) {
    if (!path.length() || _count >= MAX_FAVORITES) return false;
    if (contains(path)) return true;
    _items[_count++] = path;
    return true;
  }

  bool removeAt(int idx) {
    if (idx < 0 || idx >= _count) return false;
    for (int i = idx; i < _count - 1; i++) _items[i] = _items[i + 1];
    _items[_count - 1] = String();
    _count--;
    return true;
  }

  bool contains(const String& path) const {
    for (int i = 0; i < _count; i++) {
      if (_items[i] == path) return true;
    }
    return false;
  }

  void clear() {
    for (int i = 0; i < _count; i++) _items[i] = String();
    _count = 0;
  }

  int size() const { return _count; }

  String at(int idx) const {
    if (idx < 0 || idx >= _count) return String("");
    return _items[idx];
  }

  bool save() {
    ensureDbDir();
    if (SD.exists(_path)) SD.remove(_path);
    File f = SD.open(_path, FILE_WRITE);
    if (!f) return false;
    f.println("# IQ200 FAVORITES DB v1");
    for (int i = 0; i < _count; i++) {
      f.print("F|");
      f.println(_items[i]);
      delay(0);
    }
    f.close();
    return true;
  }

  bool load() {
    File f = SD.open(_path, FILE_READ);
    if (!f) return false;
    clear();
    while (f.available()) {
      String line = f.readStringUntil('\n');
      line.trim();
      if (!line.length() || line[0] == '#') { delay(0); continue; }
      if (line.startsWith("F|")) add(line.substring(2));
      delay(0);
    }
    f.close();
    return true;
  }

  bool removeDb() {
    clear();
    if (SD.exists(_path)) return SD.remove(_path);
    return true;
  }

  const char* path() const { return _path; }

private:
  String _items[MAX_FAVORITES];
  int _count = 0;
  const char* _path = "/iq200/db/favorites/favorites.db";

  void ensureDbDir() {
    if (!SD.exists("/iq200")) SD.mkdir("/iq200");
    if (!SD.exists("/iq200/db")) SD.mkdir("/iq200/db");
    if (!SD.exists("/iq200/db/favorites")) SD.mkdir("/iq200/db/favorites");
  }
};
