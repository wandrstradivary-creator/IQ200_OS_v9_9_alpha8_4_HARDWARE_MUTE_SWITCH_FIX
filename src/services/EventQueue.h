#pragma once
#include <Arduino.h>

enum IQEventType : uint8_t {
  EVT_NONE = 0,
  EVT_WIFI_DONE,
  EVT_SD_DONE,
  EVT_AUDIO_DONE,
  EVT_INDEX_DONE,
  EVT_AUDIO_STREAM_DONE,
  EVT_WAV_OPENED,
  EVT_WAV_STARTED,
  EVT_WAV_PROGRESS,
  EVT_WAV_STOPPED,
  EVT_MONITOR,
  EVT_ERROR
};

struct IQEvent {
  IQEventType type = EVT_NONE;
  int value = 0;
  uint64_t value64 = 0;
  char message[64] = {0};
};
