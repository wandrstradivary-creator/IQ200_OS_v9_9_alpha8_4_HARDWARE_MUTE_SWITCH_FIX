#pragma once
#include <Arduino.h>
#include "RuntimeState.h"

// IQ200 OS v7.3 Media Framework Foundation
// Lightweight facade above the current RT WAV player path.
// UI and future codecs should talk to MediaEngine/RuntimeState, not directly to WAV internals.

enum MediaCodec : uint8_t {
  MEDIA_CODEC_NONE = 0,
  MEDIA_CODEC_WAV  = 1,
  MEDIA_CODEC_MP3  = 2,
  MEDIA_CODEC_FLAC = 3,
  MEDIA_CODEC_OPUS = 4,
  MEDIA_CODEC_OGG  = 5,
  MEDIA_CODEC_AAC  = 6,
  MEDIA_CODEC_RADIO = 7
};

enum MediaState : uint8_t {
  MEDIA_STATE_READY = 0,
  MEDIA_STATE_LOADING,
  MEDIA_STATE_PLAYING,
  MEDIA_STATE_STOPPED,
  MEDIA_STATE_ERROR
};

struct MediaInfo {
  char title[64];
  char path[192];
  MediaCodec codec;
  MediaState state;
  uint32_t sampleRate;
  uint8_t channels;
  uint8_t bits;
  uint32_t dataSize;
  uint32_t playedBytes;
  uint8_t progress;
  uint8_t vuLeft;
  uint8_t vuRight;
  uint8_t bufferHealth;
  uint32_t underruns;
  uint32_t shortWrites;
};

class MediaEngine {
  RuntimeState* rt = nullptr;

  static void copyText(char* dst, size_t dstLen, const char* src) {
    if (!dst || dstLen == 0) return;
    if (!src) src = "";
    strncpy(dst, src, dstLen - 1);
    dst[dstLen - 1] = 0;
  }

public:
  void begin(RuntimeState& state) {
    rt = &state;
    copyText(rt->mediaTitle, sizeof(rt->mediaTitle), "Ready");
    copyText(rt->mediaPath, sizeof(rt->mediaPath), "");
    rt->mediaCodec = MEDIA_CODEC_NONE;
    rt->mediaState = MEDIA_STATE_READY;
  }

  bool playDefaultWav() {
    if (!rt) return false;
    if (rt->audioPlaying || rt->rtAudioTaskRunning || rt->audioBusy || rt->wavPlayRequest) {
      return false;
    }
    copyText(rt->mediaTitle, sizeof(rt->mediaTitle), "test.wav");
    copyText(rt->mediaPath, sizeof(rt->mediaPath), "/test.wav");
    rt->mediaCodec = MEDIA_CODEC_WAV;
    rt->mediaState = MEDIA_STATE_LOADING;
    rt->wavPlayRequest = true;
    return true;
  }

  void stop() {
    if (!rt) return;
    rt->wavStopRequest = true;
    rt->mediaState = MEDIA_STATE_STOPPED;
  }

  void mirrorFromRuntime() {
    if (!rt) return;

    // v9.1.1: The current RT mirror is WAV-backed. Do not use the WAV mirror
    // to zero progress/state for FLAC/OPUS/MP3/OGG/AAC while those decoders are
    // not built yet; SmartResume may have restored a real position for them.
    if (rt->mediaCodec != MEDIA_CODEC_WAV) {
      if (rt->mediaState == MEDIA_STATE_LOADING && !rt->audioBusy) rt->mediaState = MEDIA_STATE_READY;
      rt->mediaBufferHealth = rt->audioHealth;
      rt->mediaUnderruns = rt->audioUnderruns;
      rt->mediaShortWrites = rt->audioShortWrites;
      return;
    }

    if (rt->audioPlaying) rt->mediaState = MEDIA_STATE_PLAYING;
    else if (rt->wavValid) rt->mediaState = MEDIA_STATE_STOPPED;
    else if (rt->mediaState != MEDIA_STATE_LOADING) rt->mediaState = MEDIA_STATE_READY;

    rt->mediaSampleRate = rt->wavSampleRate;
    rt->mediaChannels = rt->wavChannels;
    rt->mediaDataSize = rt->wavDataSize;
    rt->mediaPlayedBytes = rt->wavPlayedBytes;
    rt->mediaProgress = rt->audioPlaying ? rt->wavProgress : rt->wavProgress;
    rt->mediaVuLeft = rt->wavVuLeft;
    rt->mediaVuRight = rt->wavVuRight;
    rt->mediaBufferHealth = rt->audioHealth;
    rt->mediaUnderruns = rt->audioUnderruns;
    rt->mediaShortWrites = rt->audioShortWrites;
  }

  MediaInfo info() const {
    MediaInfo mi{};
    if (!rt) return mi;
    copyText(mi.title, sizeof(mi.title), rt->mediaTitle);
    copyText(mi.path, sizeof(mi.path), rt->mediaPath);
    mi.codec = (MediaCodec)rt->mediaCodec;
    mi.state = (MediaState)rt->mediaState;
    mi.sampleRate = rt->mediaSampleRate;
    mi.channels = rt->mediaChannels;
    mi.bits = rt->mediaBits;
    mi.dataSize = rt->mediaDataSize;
    mi.playedBytes = rt->mediaPlayedBytes;
    mi.progress = rt->mediaProgress;
    mi.vuLeft = rt->mediaVuLeft;
    mi.vuRight = rt->mediaVuRight;
    mi.bufferHealth = rt->mediaBufferHealth;
    mi.underruns = rt->mediaUnderruns;
    mi.shortWrites = rt->mediaShortWrites;
    return mi;
  }

  static const char* codecName(uint8_t codec) {
    switch (codec) {
      case MEDIA_CODEC_WAV: return "WAV";
      case MEDIA_CODEC_MP3: return "MP3";
      case MEDIA_CODEC_FLAC: return "FLAC";
      case MEDIA_CODEC_OPUS: return "OPUS";
      case MEDIA_CODEC_OGG: return "OGG";
      case MEDIA_CODEC_AAC: return "AAC";
      case MEDIA_CODEC_RADIO: return "RADIO";
      default: return "NONE";
    }
  }

  static const char* stateName(uint8_t state) {
    switch (state) {
      case MEDIA_STATE_LOADING: return "LOADING";
      case MEDIA_STATE_PLAYING: return "PLAYING";
      case MEDIA_STATE_STOPPED: return "STOPPED";
      case MEDIA_STATE_ERROR: return "ERROR";
      default: return "READY";
    }
  }
};
