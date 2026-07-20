#pragma once
#include <Arduino.h>
#include <SD.h>

// IQ200 OS v8.0.4
// Lightweight queue foundation + Smart Shuffle / Repeat modes.
// Header-only to keep PlatformIO integration simple.
class QueueManager {
public:
  static const int MAX_QUEUE = 128;
  static const int REPEAT_OFF = 0;
  static const int REPEAT_ONE = 1;
  static const int REPEAT_ALL = 2;


  bool add(const String& path) {
    if (!path.length() || _count >= MAX_QUEUE) return false;
    _items[_count++] = path;
    if (_index < 0) _index = 0;
    return true;
  }

  bool removeAt(int idx) {
    if (idx < 0 || idx >= _count) return false;
    for (int i = idx; i < _count - 1; i++) _items[i] = _items[i + 1];
    _count--;
    if (_count <= 0) { _count = 0; _index = -1; return true; }
    if (_index >= _count) _index = _count - 1;
    return true;
  }

  void clear() {
    for (int i = 0; i < _count; i++) _items[i] = String();
    _count = 0;
    _index = -1;
  }

  int size() const { return _count; }
  int index() const { return _index; }
  bool empty() const { return _count == 0; }

  bool shuffleSmart() const { return _shuffleSmart; }
  int repeatMode() const { return _repeatMode; }

  void setShuffleSmart(bool enabled) { _shuffleSmart = enabled; }
  void toggleShuffleSmart() { _shuffleSmart = !_shuffleSmart; }

  void setRepeatMode(int mode) {
    if (mode < REPEAT_OFF) mode = REPEAT_OFF;
    if (mode > REPEAT_ALL) mode = REPEAT_ALL;
    _repeatMode = mode;
  }

  const char* repeatName() const {
    if (_repeatMode == REPEAT_ONE) return "ONE";
    if (_repeatMode == REPEAT_ALL) return "ALL";
    return "OFF";
  }


  String current() const {
    if (_index < 0 || _index >= _count) return String("");
    return _items[_index];
  }

  String at(int idx) const {
    if (idx < 0 || idx >= _count) return String("");
    return _items[idx];
  }

  String next() {
    if (_count <= 0) return String("");
    if (_index < 0) _index = 0;

    if (_repeatMode == REPEAT_ONE) {
      return current();
    }

    if (_shuffleSmart && _count > 1) {
      _index = pickSmartShuffleIndex();
      rememberRecent(_index);
      return current();
    }

    _index++;
    if (_index >= _count) {
      if (_repeatMode == REPEAT_ALL) _index = 0;
      else { _index = _count - 1; return String(""); }
    }
    rememberRecent(_index);
    return current();
  }

  String prev() {
    if (_count <= 0) return String("");
    _index--;
    if (_index < 0) _index = _count - 1;
    return current();
  }

  bool save(const char* path = "/iq200/db/queue/queue.db") {
    ensureDbDir();
    File f = SD.open(path, FILE_WRITE);
    if (!f) return false;
    f.println("# IQ200 QUEUE DB v1");
    f.printf("INDEX=%d\n", _index);
    f.printf("SHUFFLE=%d\n", _shuffleSmart ? 1 : 0);
    f.printf("REPEAT=%d\n", _repeatMode);
    for (int i = 0; i < _count; i++) {
      f.print("Q|");
      f.println(_items[i]);
      delay(0);
    }
    f.close();
    return true;
  }

  bool load(const char* path = "/iq200/db/queue/queue.db") {
    // v8.2.6: missing queue.db is a normal first-boot condition.
    // Check exists() first so Arduino VFS does not print a scary open() error.
    if (!SD.exists(path)) {
      clear();
      return true;
    }
    File f = SD.open(path, FILE_READ);
    if (!f) return false;
    clear();
    int savedIndex = -1;
    int savedShuffle = 0;
    int savedRepeat = REPEAT_ALL;
    while (f.available()) {
      String line = f.readStringUntil('\n');
      line.trim();
      if (!line.length() || line[0] == '#') continue;
      if (line.startsWith("INDEX=")) {
        savedIndex = line.substring(6).toInt();
      } else if (line.startsWith("SHUFFLE=")) {
        savedShuffle = line.substring(8).toInt();
      } else if (line.startsWith("REPEAT=")) {
        savedRepeat = line.substring(7).toInt();
      } else if (line.startsWith("Q|")) {
        add(line.substring(2));
      }
      delay(0);
    }
    f.close();
    _shuffleSmart = savedShuffle != 0;
    setRepeatMode(savedRepeat);
    if (_count > 0) {
      if (savedIndex < 0 || savedIndex >= _count) savedIndex = 0;
      _index = savedIndex;
      rememberRecent(_index);
    }
    return true;
  }

private:
  String _items[MAX_QUEUE];
  int _count = 0;
  int _index = -1;
  bool _shuffleSmart = false;
  int _repeatMode = REPEAT_ALL; // preserve old wrap-around behavior by default
  int _recent[32];
  int _recentCount = 0;
  uint32_t _rng = 0x1A2B3C4D;

  uint32_t nextRand() {
    // Small deterministic PRNG; avoids pulling in additional dependencies.
    _rng ^= _rng << 13;
    _rng ^= _rng >> 17;
    _rng ^= _rng << 5;
    if (_rng == 0) _rng = micros() ^ millis() ^ 0xA5A55A5A;
    return _rng;
  }

  bool wasRecent(int idx) const {
    for (int i = 0; i < _recentCount; i++) if (_recent[i] == idx) return true;
    return false;
  }

  void rememberRecent(int idx) {
    if (idx < 0) return;
    const int maxRecent = 32;
    if (_recentCount < maxRecent) {
      _recent[_recentCount++] = idx;
      return;
    }
    for (int i = 1; i < maxRecent; i++) _recent[i - 1] = _recent[i];
    _recent[maxRecent - 1] = idx;
  }

  int pickSmartShuffleIndex() {
    if (_count <= 1) return 0;
    int candidate = _index;
    int tries = min(_count * 2, 64);
    for (int i = 0; i < tries; i++) {
      int n = (int)(nextRand() % (uint32_t)_count);
      if (n != _index && !wasRecent(n)) return n;
      candidate = n;
    }
    // Fallback: walk to the first non-current index.
    for (int i = 0; i < _count; i++) {
      int n = (_index + 1 + i) % _count;
      if (n != _index) return n;
    }
    return _index;
  }

  void ensureDbDir() {
    if (!SD.exists("/iq200")) SD.mkdir("/iq200");
    if (!SD.exists("/iq200/db")) SD.mkdir("/iq200/db");
    if (!SD.exists("/iq200/db/queue")) SD.mkdir("/iq200/db/queue");
  }
};
