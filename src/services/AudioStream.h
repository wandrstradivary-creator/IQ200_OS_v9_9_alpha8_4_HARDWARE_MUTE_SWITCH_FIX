#pragma once
#include <Arduino.h>
#include <SD.h>
#include "AudioEngine.h"
#include "WavParser.h"

class AudioStream {
  File file;
  bool playing = false;
  uint32_t bytesRead = 0;
  WavInfo wav;

public:
  bool openWav(const char* path) {
    stop();
    file = SD.open(path, FILE_READ);
    if (!file) return false;

    if (!WavParser::parse(file, wav)) {
      file.close();
      playing = false;
      return false;
    }

    file.seek(wav.dataOffset);
    bytesRead = 0;
    playing = true;
    return true;
  }

  const WavInfo& info() const { return wav; }

  void stop() {
    if (file) file.close();
    playing = false;
    bytesRead = 0;
  }

  bool isPlaying() const { return playing; }
  uint32_t readBytes() const { return bytesRead; }

  size_t pump(uint8_t* dst, size_t maxLen) {
    if (!playing || !file) return 0;
    size_t n = file.read(dst, maxLen);
    bytesRead += n;
    if (n == 0 || !file.available()) {
      stop();
    }
    return n;
  }
};
