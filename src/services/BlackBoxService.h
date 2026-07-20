#pragma once

#include <Arduino.h>
#include <stdlib.h>

class BlackBoxService {
public:
  struct Entry {
    uint32_t ms;
    int32_t value;
    uint64_t value64;
    uint16_t seq;
    uint8_t core;
    uint8_t kind;
    char tag[12];
    char message[36];
  };

  enum Kind : uint8_t {
    KIND_SYSTEM = 0,
    KIND_EVENT = 1,
    KIND_PLAYER = 2,
    KIND_SD = 3,
    KIND_COMMAND = 4,
    KIND_ERROR = 5
  };

  bool begin(size_t capacity = 512) {
    if (_entries) return true;
    if (capacity < 64) capacity = 64;
    _capacity = capacity;
    _entries = static_cast<Entry*>(ps_malloc(sizeof(Entry) * _capacity));
    _inPsram = _entries != nullptr;
    if (!_entries) _entries = static_cast<Entry*>(malloc(sizeof(Entry) * _capacity));
    if (!_entries) {
      _capacity = 0;
      return false;
    }
    memset(_entries, 0, sizeof(Entry) * _capacity);
    _enabled = true;
    record(KIND_SYSTEM, "BB", 1, _capacity, _inPsram ? "started PSRAM" : "started DRAM");
    return true;
  }

  void setEnabled(bool enabled) {
    _enabled = enabled;
    if (enabled) record(KIND_SYSTEM, "BB", 1, _capacity, "enabled");
  }

  bool enabled() const { return _enabled; }
  bool inPsram() const { return _inPsram; }
  size_t capacity() const { return _capacity; }
  uint32_t totalWritten() const { return _totalWritten; }
  size_t count() const { return _count; }

  void clear() {
    portENTER_CRITICAL(&_mux);
    _head = 0;
    _count = 0;
    _totalWritten = 0;
    portEXIT_CRITICAL(&_mux);
    record(KIND_SYSTEM, "BB", 0, 0, "cleared");
  }

  void record(Kind kind, const char* tag, int32_t value, uint64_t value64, const char* message) {
    if (!_enabled || !_entries || !_capacity) return;
    portENTER_CRITICAL(&_mux);
    Entry& e = _entries[_head];
    memset(&e, 0, sizeof(e));
    e.ms = millis();
    e.value = value;
    e.value64 = value64;
    e.seq = static_cast<uint16_t>(_totalWritten & 0xFFFFU);
    e.core = static_cast<uint8_t>(xPortGetCoreID());
    e.kind = static_cast<uint8_t>(kind);
    copy(e.tag, sizeof(e.tag), tag);
    copy(e.message, sizeof(e.message), message);
    _head = (_head + 1U) % _capacity;
    if (_count < _capacity) ++_count;
    ++_totalWritten;
    portEXIT_CRITICAL(&_mux);
  }

  void status(Print& out) const {
    out.printf("[BB] enabled=%d storage=%s entries=%u/%u total=%lu bytes=%u\n",
               _enabled ? 1 : 0, _inPsram ? "PSRAM" : "DRAM",
               (unsigned)_count, (unsigned)_capacity,
               (unsigned long)_totalWritten,
               (unsigned)(_capacity * sizeof(Entry)));
  }

  void dump(Print& out, size_t limit = 0) {
    if (!_entries || !_capacity) {
      out.println("[BB] unavailable");
      return;
    }
    Entry snapshot;
    size_t countNow;
    size_t start;
    portENTER_CRITICAL(&_mux);
    countNow = _count;
    if (limit && countNow > limit) countNow = limit;
    start = (_head + _capacity - countNow) % _capacity;
    portEXIT_CRITICAL(&_mux);

    out.printf("========== BLACK BOX (%u entries) ==========\n", (unsigned)countNow);
    for (size_t i = 0; i < countNow; ++i) {
      portENTER_CRITICAL(&_mux);
      snapshot = _entries[(start + i) % _capacity];
      portEXIT_CRITICAL(&_mux);
      out.printf("[BB][%05u] t=%lu c=%u k=%u %-10s v=%ld v64=%llu %s\n",
                 (unsigned)snapshot.seq, (unsigned long)snapshot.ms,
                 (unsigned)snapshot.core, (unsigned)snapshot.kind,
                 snapshot.tag, (long)snapshot.value,
                 (unsigned long long)snapshot.value64, snapshot.message);
      if ((i & 15U) == 15U) delay(1);
    }
    out.println("========== END BLACK BOX ==========");
  }

private:
  static void copy(char* dst, size_t n, const char* src) {
    if (!dst || !n) return;
    if (!src) src = "";
    strncpy(dst, src, n - 1U);
    dst[n - 1U] = 0;
  }

  Entry* _entries = nullptr;
  size_t _capacity = 0;
  size_t _head = 0;
  size_t _count = 0;
  uint32_t _totalWritten = 0;
  bool _enabled = false;
  bool _inPsram = false;
  mutable portMUX_TYPE _mux = portMUX_INITIALIZER_UNLOCKED;
};
