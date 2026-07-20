#pragma once
#include <Arduino.h>
#include "driver/i2s.h"
#include "iq200_pins.h"

class AudioEngine {
  bool installed = false;
  int volume = 8;
  uint32_t currentRate = 44100;
  uint8_t* volumeBuf = nullptr;
  size_t volumeBufSize = 0;
  esp_err_t lastInstallErr = ESP_OK;
  esp_err_t lastPinErr = ESP_OK;
  esp_err_t lastClockErr = ESP_OK;

  bool eqEnabled = true;
  int8_t eqBassDb = 0;
  int8_t eqMidDb = 0;
  int8_t eqTrebleDb = 0;
  char eqPresetName[16] = "flat";
  int32_t eqLowL = 0, eqLowR = 0;
  int32_t eqSmoothL = 0, eqSmoothR = 0;

  void ensureVolumeBuf(size_t need) {
    if (volumeBuf && volumeBufSize >= need) return;
    if (volumeBuf) { free(volumeBuf); volumeBuf = nullptr; volumeBufSize = 0; }
    volumeBuf = (uint8_t*)ps_malloc(need);
    if (!volumeBuf) volumeBuf = (uint8_t*)malloc(need);
    if (volumeBuf) volumeBufSize = need;
  }

  static int16_t clamp16(int32_t v) {
    if (v > 32767) return 32767;
    if (v < -32768) return -32768;
    return (int16_t)v;
  }

  static int32_t eqGainQ8(int8_t db) {
    // Low-cost approximation, safe for Core0 RT audio.
    int32_t g = 256 + (int32_t)db * 16;
    if (g < 64) g = 64;
    if (g > 448) g = 448;
    return g;
  }

  const uint8_t* scaledPCM16StereoChunk(const uint8_t* data, size_t len) {
    if (volume >= 99 && (!eqEnabled || (eqBassDb == 0 && eqMidDb == 0 && eqTrebleDb == 0))) return data;
    ensureVolumeBuf(len);
    if (!volumeBuf) return data;
    const int16_t* src = (const int16_t*)data;
    int16_t* dst = (int16_t*)volumeBuf;
    const size_t frames = len / 4;
    const int32_t gb = eqGainQ8(eqBassDb);
    const int32_t gm = eqGainQ8(eqMidDb);
    const int32_t gt = eqGainQ8(eqTrebleDb);
    for (size_t i = 0; i < frames; ++i) {
      int32_t inL = src[i*2];
      int32_t inR = src[i*2+1];
      int32_t outL = inL, outR = inR;
      if (eqEnabled) {
        eqLowL += (inL - eqLowL) >> 5;
        eqLowR += (inR - eqLowR) >> 5;
        eqSmoothL += (inL - eqSmoothL) >> 2;
        eqSmoothR += (inR - eqSmoothR) >> 2;
        const int32_t lowL = eqLowL, lowR = eqLowR;
        const int32_t highL = inL - eqSmoothL, highR = inR - eqSmoothR;
        const int32_t midL = eqSmoothL - lowL, midR = eqSmoothR - lowR;
        outL = (lowL*gb + midL*gm + highL*gt) >> 8;
        outR = (lowR*gb + midR*gm + highR*gt) >> 8;
      }
      outL = (outL * volume) / 100;
      outR = (outR * volume) / 100;
      dst[i*2] = clamp16(outL);
      dst[i*2+1] = clamp16(outR);
    }
    return volumeBuf;
  }

public:
  bool begin(uint32_t sampleRate = 44100) {
    if (installed && currentRate == sampleRate) return true;

    if (installed) {
      i2s_zero_dma_buffer(I2S_NUM_0);
      i2s_driver_uninstall(I2S_NUM_0);
      installed = false;
    }

    currentRate = sampleRate;

    i2s_config_t cfg = {};
    cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
    cfg.sample_rate = sampleRate;
    cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    cfg.dma_buf_count = 16;
    cfg.dma_buf_len = 512;
    cfg.use_apll = false;
    cfg.tx_desc_auto_clear = true;
    cfg.fixed_mclk = 0;

    i2s_pin_config_t pins = {};
    pins.bck_io_num = IQ200_I2S_BCK;
    pins.ws_io_num = IQ200_I2S_LRCK;
    pins.data_out_num = IQ200_I2S_DOUT;
    pins.data_in_num = I2S_PIN_NO_CHANGE;

    const uint32_t heapBefore = ESP.getFreeHeap();
    const uint32_t psramBefore = ESP.getFreePsram();
    Serial.printf("[PIPE][I2S] begin rate=%lu installed=%d current=%lu heap=%lu psram=%lu\n",
                  (unsigned long)sampleRate, installed ? 1 : 0, (unsigned long)currentRate,
                  (unsigned long)heapBefore, (unsigned long)psramBefore);

    esp_err_t e1 = i2s_driver_install(I2S_NUM_0, &cfg, 0, nullptr);
    // A stale driver can survive if another playback path uninstalled it
    // without updating this AudioEngine instance. Recover once and log it.
    if (e1 == ESP_ERR_INVALID_STATE) {
      Serial.println("[PIPE][I2S] install INVALID_STATE; forcing stale-driver uninstall and retry");
      i2s_driver_uninstall(I2S_NUM_0);
      e1 = i2s_driver_install(I2S_NUM_0, &cfg, 0, nullptr);
    }
    esp_err_t e2 = (e1 == ESP_OK) ? i2s_set_pin(I2S_NUM_0, &pins) : ESP_FAIL;
    esp_err_t e3 = (e1 == ESP_OK && e2 == ESP_OK)
                     ? i2s_set_clk(I2S_NUM_0, sampleRate, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_STEREO)
                     : ESP_FAIL;
    if (e1 == ESP_OK) i2s_zero_dma_buffer(I2S_NUM_0);

    lastInstallErr = e1;
    lastPinErr = e2;
    lastClockErr = e3;
    installed = (e1 == ESP_OK && e2 == ESP_OK && e3 == ESP_OK);
    Serial.printf("[PIPE][I2S] result ok=%d install=%d pin=%d clk=%d heap=%lu psram=%lu\n",
                  installed ? 1 : 0, (int)e1, (int)e2, (int)e3,
                  (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getFreePsram());
    return installed;
  }

  void stop() {
    if (installed) {
      i2s_zero_dma_buffer(I2S_NUM_0);
      i2s_driver_uninstall(I2S_NUM_0);
      installed = false;
    }
  }

  bool isReady() const { return installed; }
  esp_err_t installError() const { return lastInstallErr; }
  esp_err_t pinError() const { return lastPinErr; }
  esp_err_t clockError() const { return lastClockErr; }

  void setVolume(int v) {
    if (v < 0) v = 0;
    if (v > 100) v = 100;
    volume = v;
  }

  int getVolume() const { return volume; }

  void setEqualizer(bool enabled, int bassDb, int midDb, int trebleDb, const char* preset = "custom") {
    eqEnabled = enabled;
    eqBassDb = constrain(bassDb, -12, 12);
    eqMidDb = constrain(midDb, -12, 12);
    eqTrebleDb = constrain(trebleDb, -12, 12);
    strncpy(eqPresetName, preset ? preset : "custom", sizeof(eqPresetName)-1);
    eqPresetName[sizeof(eqPresetName)-1] = 0;
    eqLowL = eqLowR = eqSmoothL = eqSmoothR = 0;
    Serial.printf("[EQ] preset=%s enabled=%d bass=%d mid=%d treble=%d\n", eqPresetName, eqEnabled?1:0, eqBassDb, eqMidDb, eqTrebleDb);
  }

  bool setEqualizerPreset(const String& requested) {
    String p = requested; p.toLowerCase(); p.trim();
    if (p == "off") { setEqualizer(false,0,0,0,"off"); return true; }
    if (p == "flat") { setEqualizer(true,0,0,0,"flat"); return true; }
    if (p == "rock") { setEqualizer(true,5,-2,4,"rock"); return true; }
    if (p == "pop") { setEqualizer(true,2,3,2,"pop"); return true; }
    if (p == "jazz") { setEqualizer(true,3,1,4,"jazz"); return true; }
    if (p == "classic" || p == "classical") { setEqualizer(true,2,0,3,"classic"); return true; }
    if (p == "bass") { setEqualizer(true,7,0,-1,"bass"); return true; }
    if (p == "treble") { setEqualizer(true,-1,0,7,"treble"); return true; }
    if (p == "vocal") { setEqualizer(true,-2,6,2,"vocal"); return true; }
    return false;
  }

  bool equalizerEnabled() const { return eqEnabled; }
  int equalizerBass() const { return eqBassDb; }
  int equalizerMid() const { return eqMidDb; }
  int equalizerTreble() const { return eqTrebleDb; }
  const char* equalizerPreset() const { return eqPresetName; }
  void printEqualizer(Stream& out) const {
    out.printf("[EQ] preset=%s enabled=%d bass=%d mid=%d treble=%d\n", eqPresetName, eqEnabled?1:0, eqBassDb, eqMidDb, eqTrebleDb);
    out.println("[EQ] presets: flat rock pop jazz classic bass treble vocal off");
    out.println("[EQ] custom: eq custom <bass -12..12> <mid -12..12> <treble -12..12>");
  }

  void primeSilence(uint16_t ms = 80) {
    if (!begin(currentRate)) return;
    static uint8_t zeros[512] = {0};
    uint32_t endAt = millis() + ms;
    while ((int32_t)(millis() - endAt) < 0) {
      size_t written = 0;
      i2s_write(I2S_NUM_0, zeros, sizeof(zeros), &written, pdMS_TO_TICKS(20));
    }
  }

  void fadeInPCM16Stereo(uint8_t* data, size_t len, size_t framesToFade = 256) {
    if (!data || len < 4) return;
    len &= ~((size_t)3);
    int16_t* s = (int16_t*)data;
    size_t frames = len / 4;
    if (frames > framesToFade) frames = framesToFade;

    for (size_t i = 0; i < frames; i++) {
      int32_t gain = (int32_t)i * 1024 / (int32_t)frames;
      s[i * 2]     = (int16_t)((int32_t)s[i * 2] * gain / 1024);
      s[i * 2 + 1] = (int16_t)((int32_t)s[i * 2 + 1] * gain / 1024);
    }
  }

  bool writePCM(const uint8_t* data, size_t len, size_t* writtenOut = nullptr) {
    if (!begin(currentRate)) return false;
    if (!data || len == 0) return false;

    len &= ~((size_t)3); // stereo 16-bit frame alignment
    if (len == 0) return false;

    size_t written = 0;
    const uint8_t* out = scaledPCM16StereoChunk(data, len);
    esp_err_t err = i2s_write(I2S_NUM_0, out, len, &written, pdMS_TO_TICKS(100));
    if (writtenOut) *writtenOut = written;
    return err == ESP_OK && written > 0;
  }

  bool writePCMAll(const uint8_t* data, size_t len, size_t* writtenOut = nullptr) {
    if (!begin(currentRate)) return false;
    if (!data || len == 0) return false;

    len &= ~((size_t)3);
    if (len == 0) return false;

    size_t total = 0;
    uint32_t start = millis();

    while (total < len) {
      size_t w = 0;
      size_t remain = len - total;
      size_t chunk = remain > 4096 ? 4096 : remain;
      chunk &= ~((size_t)3);
      if (chunk == 0) chunk = remain & ~((size_t)3);
      const uint8_t* out = scaledPCM16StereoChunk(data + total, chunk);
      esp_err_t err = i2s_write(I2S_NUM_0, out, chunk, &w, pdMS_TO_TICKS(250));
      if (err != ESP_OK) {
        if (writtenOut) *writtenOut = total;
        return false;
      }
      if (w == 0) {
        if (millis() - start > 1000) {
          if (writtenOut) *writtenOut = total;
          return false;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
        continue;
      }
      total += w;
      start = millis();
    }

    if (writtenOut) *writtenOut = total;
    return true;
  }

  void toneTest(uint16_t ms = 300) {
    if (!begin(44100)) return;

    const int samples = 44100 * ms / 1000;
    for (int i = 0; i < samples; i++) {
      int16_t s = ((i / 50) & 1) ? 1800 : -1800;
      int16_t stereo[2] = {s, s};
      size_t written = 0;
      i2s_write(I2S_NUM_0, stereo, sizeof(stereo), &written, pdMS_TO_TICKS(20));
    }
  }
};
