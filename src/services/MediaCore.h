#pragma once
#include <Arduino.h>
#include "RuntimeState.h"
#include "QueueManager.h"

// IQ200 OS v8.0.4 Media Core foundation.
// For now this is a lightweight coordinator layer. It centralizes future media control
// without disturbing the proven RT Audio path.
class MediaCore {
public:
  void begin(RuntimeState& state, QueueManager& queue) {
    _rt = &state;
    _queue = &queue;
    _started = true;
  }

  bool started() const { return _started; }

  void requestPlay() {
    if (!_rt) return;
    _rt->wavPlayRequest = true;
  }

  void requestStop() {
    if (!_rt) return;
    _rt->wavStopRequest = true;
  }

  bool queueCurrentToRuntime() {
    if (!_rt || !_queue || _queue->empty()) return false;
    String p = _queue->current();
    if (!p.length()) return false;
    strncpy(_rt->mediaPath, p.c_str(), sizeof(_rt->mediaPath) - 1);
    _rt->mediaPath[sizeof(_rt->mediaPath) - 1] = 0;
    strncpy(_rt->playlistCurrent, p.c_str(), sizeof(_rt->playlistCurrent) - 1);
    _rt->playlistCurrent[sizeof(_rt->playlistCurrent) - 1] = 0;
    _rt->queueCount = _queue->size();
    _rt->queueIndex = _queue->index();
    _rt->queueShuffleSmart = _queue->shuffleSmart();
    _rt->queueRepeatMode = _queue->repeatMode();
    return true;
  }

  void mirrorQueue() {
    if (!_rt || !_queue) return;
    _rt->queueCount = _queue->size();
    _rt->queueIndex = _queue->index();
    _rt->queueShuffleSmart = _queue->shuffleSmart();
    _rt->queueRepeatMode = _queue->repeatMode();
  }

private:
  RuntimeState* _rt = nullptr;
  QueueManager* _queue = nullptr;
  bool _started = false;
};
