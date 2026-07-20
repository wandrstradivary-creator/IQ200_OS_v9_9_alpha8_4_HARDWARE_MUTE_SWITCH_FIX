#pragma once
#include <Arduino.h>
#include "RuntimeState.h"
#include "MessageBroker.h"

// IQ200 OS v8.0.2
// Event Bus layer over the existing MessageBroker/FreeRTOS queue.
// It centralizes event counters and keeps modules decoupled without touching RT Audio.
class EventBus {
public:
  void begin(RuntimeState& state, MessageBroker& broker) {
    _rt = &state;
    _broker = &broker;
  }

  bool post(IQEventType type, int value, uint64_t value64, const char* msg) {
    if (!_broker) return false;
    if (_rt) {
      _rt->eventBusPosts++;
      classify(type);
    }
    bool ok = _broker->postEvent(type, value, value64, msg);
    if (!ok && _rt) _rt->eventBusDrops++;
    return ok;
  }

  static const char* name(IQEventType type) {
    switch (type) {
      case EVT_WIFI_DONE: return "WIFI_DONE";
      case EVT_SD_DONE: return "SD_DONE";
      case EVT_AUDIO_DONE: return "AUDIO_DONE";
      case EVT_INDEX_DONE: return "INDEX_DONE";
      case EVT_AUDIO_STREAM_DONE: return "AUDIO_STREAM_DONE";
      case EVT_WAV_OPENED: return "WAV_OPENED";
      case EVT_WAV_STARTED: return "WAV_STARTED";
      case EVT_WAV_PROGRESS: return "WAV_PROGRESS";
      case EVT_WAV_STOPPED: return "WAV_STOPPED";
      case EVT_MONITOR: return "MONITOR";
      case EVT_ERROR: return "ERROR";
      default: return "NONE";
    }
  }

private:
  RuntimeState* _rt = nullptr;
  MessageBroker* _broker = nullptr;

  void classify(IQEventType type) {
    if (!_rt) return;
    if (type == EVT_WAV_OPENED || type == EVT_WAV_STARTED || type == EVT_WAV_PROGRESS || type == EVT_WAV_STOPPED || type == EVT_AUDIO_DONE || type == EVT_AUDIO_STREAM_DONE) {
      _rt->mediaEventCount++;
    } else if (type == EVT_INDEX_DONE || type == EVT_SD_DONE) {
      _rt->dbEventCount++;
    } else if (type == EVT_MONITOR) {
      _rt->scanEventCount++;
    }
  }
};
