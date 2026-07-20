#pragma once
#include <Arduino.h>
#include "MediaEngine.h"

// IQ200 OS v7.3 Media Framework
// Codec-neutral interfaces. Current stable playback remains WAV RT path.
// MP3/FLAC/Radio can be added later without changing Player UI.

class IDecoder {
public:
  virtual ~IDecoder() {}
  virtual bool open(const char* path) = 0;
  virtual size_t decode(uint8_t* out, size_t maxLen) = 0;
  virtual void close() = 0;
  virtual MediaInfo info() const = 0;
};

struct MediaTrack {
  char path[192];
  char title[64];
  MediaCodec codec;
};

static inline MediaCodec mediaCodecFromPath(const char* path) {
  if (!path) return MEDIA_CODEC_NONE;
  String p(path);
  p.toLowerCase();
  if (p.endsWith(".flac")) return MEDIA_CODEC_FLAC;
  if (p.endsWith(".opus")) return MEDIA_CODEC_OPUS;
  if (p.endsWith(".mp3")) return MEDIA_CODEC_MP3;
  if (p.endsWith(".ogg") || p.endsWith(".oga")) return MEDIA_CODEC_OGG;
  if (p.endsWith(".wav")) return MEDIA_CODEC_WAV;
  if (p.endsWith(".aac") || p.endsWith(".m4a")) return MEDIA_CODEC_AAC;
  if (p.startsWith("http://") || p.startsWith("https://")) return MEDIA_CODEC_RADIO;
  return MEDIA_CODEC_NONE;
}

static inline const char* mediaTitleFromPath(const char* path) {
  static char title[64];
  title[0] = 0;
  if (!path || !path[0]) return title;
  const char* slash = strrchr(path, '/');
  const char* base = slash ? slash + 1 : path;
  strncpy(title, base, sizeof(title) - 1);
  title[sizeof(title) - 1] = 0;
  return title;
}
