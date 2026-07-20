#pragma once
#include <Arduino.h>
#include <SD.h>

struct WavInfo {
  bool valid = false;
  uint32_t sampleRate = 0;
  uint16_t channels = 0;
  uint16_t bitsPerSample = 0;
  uint32_t dataOffset = 0;
  uint32_t dataSize = 0;
};

class WavParser {
public:
  static uint32_t rd32(File& f) {
    uint8_t b[4];
    if (f.read(b, 4) != 4) return 0;
    return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
  }

  static uint16_t rd16(File& f) {
    uint8_t b[2];
    if (f.read(b, 2) != 2) return 0;
    return (uint16_t)b[0] | ((uint16_t)b[1] << 8);
  }

  static bool parse(File& f, WavInfo& info) {
    info = WavInfo();

    if (!f || f.size() < 44) return false;
    f.seek(0);

    char riff[5] = {0};
    char wave[5] = {0};

    f.read((uint8_t*)riff, 4);
    rd32(f);
    f.read((uint8_t*)wave, 4);

    if (String(riff) != "RIFF" || String(wave) != "WAVE") return false;

    bool gotFmt = false;
    bool gotData = false;

    while (f.available()) {
      char id[5] = {0};
      if (f.read((uint8_t*)id, 4) != 4) break;
      uint32_t sz = rd32(f);
      uint32_t chunkStart = f.position();

      if (String(id) == "fmt ") {
        uint16_t audioFormat = rd16(f);
        info.channels = rd16(f);
        info.sampleRate = rd32(f);
        rd32(f); // byte rate
        rd16(f); // block align
        info.bitsPerSample = rd16(f);
        gotFmt = audioFormat == 1; // PCM
      } else if (String(id) == "data") {
        info.dataOffset = f.position();
        info.dataSize = sz;
        gotData = true;
        break;
      }

      f.seek(chunkStart + sz + (sz & 1));
    }

    info.valid = gotFmt && gotData && info.sampleRate > 0 && info.channels > 0 && info.bitsPerSample == 16;
    return info.valid;
  }
};
