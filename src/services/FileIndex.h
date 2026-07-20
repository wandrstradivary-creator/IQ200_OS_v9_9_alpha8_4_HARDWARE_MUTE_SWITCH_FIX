#pragma once
#include <Arduino.h>
#include <SD.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

class FileIndex {
  static const int MAX_ITEMS = 256;
  static const int MAX_SCAN_DEPTH = 6;
  static const int YIELD_EVERY_ITEMS = 4;

  String names[MAX_ITEMS];
  int count = 0;
  uint32_t visited = 0;

  static String joinPath(const String& dir, const String& name) {
    if (!name.length()) return dir;
    if (name[0] == '/') return name;
    if (dir == "/") return String("/") + name;
    return dir + "/" + name;
  }

  static bool isBadDirName(const String& path) {
    String p = path;
    p.toLowerCase();
    return p.indexOf("system volume information") >= 0 ||
           p.indexOf("lost.dir") >= 0 ||
           p.indexOf(".trash") >= 0 ||
           p.indexOf(".spotlight") >= 0 ||
           p.indexOf(".fseventsd") >= 0 ||
           p.indexOf("android/data") >= 0 ||
           p.indexOf("android/obb") >= 0;
  }

  void scannerYield() {
    visited++;
    if ((visited % YIELD_EVERY_ITEMS) == 0) {
      // Critical for ESP32-S3: let IDLE0 feed WDT while SD/FAT traversal is active.
      vTaskDelay(1);
    }
  }

  void scanDir(const String& dirPath, int depth) {
    if (count >= MAX_ITEMS) return;
    if (depth > MAX_SCAN_DEPTH) return;
    if (isBadDirName(dirPath)) return;

    File dir = SD.open(dirPath);
    if (!dir) {
      scannerYield();
      return;
    }

    while (count < MAX_ITEMS) {
      File f = dir.openNextFile();
      if (!f) break;

      String name = String(f.name());
      String path = joinPath(dirPath, name);

      if (f.isDirectory()) {
        if (!isBadDirName(path)) {
          f.close();
          scannerYield();
          scanDir(path, depth + 1);
        } else {
          f.close();
          scannerYield();
        }
      } else {
        names[count++] = path;
        f.close();
        scannerYield();
      }
    }

    dir.close();
    scannerYield();
  }

public:
  void clear() {
    for (int i = 0; i < MAX_ITEMS; i++) names[i] = "";
    count = 0;
    visited = 0;
  }

  int scanRoot() {
    clear();
    scanDir("/", 0);
    return count;
  }

  int size() const { return count; }

  const String& get(int i) const {
    static String empty = "";
    if (i < 0 || i >= count) return empty;
    return names[i];
  }
};
