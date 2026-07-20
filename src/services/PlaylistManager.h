#pragma once
#include <Arduino.h>

class PlaylistManager {
  static const int MAX_TRACKS = 1024;  // active RAM window; full database is SD-backed and multivolume
  String tracks[MAX_TRACKS];
  int count = 0;
  int current = 0;

public:
  void clear() { count = 0; current = 0; }

  bool add(const String& path) {
    if (count >= MAX_TRACKS) return false;
    tracks[count++] = path;
    return true;
  }

  int size() const { return count; }
  int index() const { return current; }

  bool select(int i) {
    if (i < 0 || i >= count) return false;
    current = i;
    return true;
  }

  const String& at(int i) const {
    static String empty = "";
    if (i < 0 || i >= count) return empty;
    return tracks[i];
  }

  const String& currentTrack() const {
    static String empty = "";
    if (count == 0) return empty;
    return tracks[current];
  }

  const String& next() {
    static String empty = "";
    if (count == 0) return empty;
    current = (current + 1) % count;
    return tracks[current];
  }

  const String& prev() {
    static String empty = "";
    if (count == 0) return empty;
    current = (current + count - 1) % count;
    return tracks[current];
  }
};
