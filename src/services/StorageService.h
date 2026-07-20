#pragma once
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <Preferences.h>
#include "SDManager.h"
#include "iq200_pins.h"

class StorageService {
public:
  bool ok = false;
  uint64_t mb = 0;
  uint32_t mountAttempts = 0;
  uint32_t lastMountMs = 0;

  bool mount() {
    if (ok) return true;

    mountAttempts++;
    lastMountMs = millis();

    // Shared SPI bus: ILI9488 + SD. Use the known TFT SPI pins and a separate SD CS.
    pinMode(IQ200_SD_CS, OUTPUT);
    digitalWrite(IQ200_SD_CS, HIGH);
    SPI.begin(IQ200_TFT_SCLK, IQ200_TFT_MISO, IQ200_TFT_MOSI, IQ200_SD_CS);

    // v9.8-alpha40: user-selectable SD clock stored in NVS.
    Preferences sdPrefs;
    sdPrefs.begin("iq200-sd", true);
    uint32_t preferredHz = sdPrefs.getUInt("clock", 12000000U);
    sdPrefs.end();
    if (!SDManager::supportedFrequency(preferredHz)) preferredHz = 12000000U;
    const uint32_t kSdFrequencies[] = { preferredHz, 12000000U, 10000000U };
    uint32_t selectedHz = 0;
    uint32_t previousTry = 0;
    for (uint32_t hz : kSdFrequencies) {
      if (hz == previousTry) continue;
      previousTry = hz;
      ok = SDManager::begin(IQ200_SD_CS, SPI, hz);
      if (ok) {
        selectedHz = hz;
        break;
      }
      SD.end();
      delay(30);
    }
    mb = ok ? SDManager::cardSize() / 1024 / 1024 : 0;

    Serial.printf("[SD] mount attempt=%lu ok=%d size=%llu MB cs=%d freq=%lu\n",
                  (unsigned long)mountAttempts, ok ? 1 : 0,
                  (unsigned long long)mb, IQ200_SD_CS,
                  (unsigned long)selectedHz);

    if (ok) {
      SDManager::Guard session;
      if (session) {
        if (!SD.exists("/iq200")) SD.mkdir("/iq200");
        if (!SD.exists("/iq200/db")) SD.mkdir("/iq200/db");
        if (!SD.exists("/iq200/config")) SD.mkdir("/iq200/config");
        File f = SD.open("/iq200/boot.txt", FILE_APPEND);
        if (f) {
          f.printf("IQ200 OS v9.2-alpha1 SD OK ms=%lu\n", (unsigned long)millis());
          f.close();
        }
      }
    }
    return ok;
  }

  bool mounted() const { return ok; }
};
