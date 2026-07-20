#pragma once

#include <Arduino.h>
#include "../drivers/Display.h"
#include "../drivers/Encoder.h"
#include "../services/ModeManager.h"

class ModeCenterUI {
  IQ200Display& display;
  ModeManager& modes;
  LGFX_Sprite fb;
  bool fbReady = false;
  Encoder nav{IQ200_ENC_NAV_CLK, IQ200_ENC_NAV_DT, IQ200_ENC_NAV_SW};
  Encoder confirm{IQ200_ENC_VOL_CLK, IQ200_ENC_VOL_DT, IQ200_ENC_VOL_SW};
  int selected = 0;
  uint32_t messageUntil = 0;
  char message[64] = "Rotate NAV, press to start";

  static constexpr IQ200Mode entries[4] = {
    IQ200_MODE_LOCAL_PLAYER,
    IQ200_MODE_WEBRADIO,
    IQ200_MODE_BLUETOOTH,
    IQ200_MODE_RADIO
  };

  static const char* label(IQ200Mode mode) {
    switch (mode) {
      case IQ200_MODE_LOCAL_PLAYER: return "LOCAL PLAYER";
      case IQ200_MODE_WEBRADIO: return "WEB RADIO";
      case IQ200_MODE_BLUETOOTH: return "BLUETOOTH";
      case IQ200_MODE_RADIO: return "FM / RADIO";
      default: return "UNKNOWN";
    }
  }

  template <typename Canvas>
  void drawCanvas(Canvas& canvas) {
    canvas.fillScreen(TFT_BLACK);
    canvas.setTextColor(TFT_CYAN, TFT_BLACK);
    canvas.setTextSize(3);
    canvas.setCursor(18, 16);
    canvas.print("IQ200 MODE CENTER");
    canvas.setTextSize(1);
    canvas.setTextColor(0x8410, TFT_BLACK);
    canvas.setCursor(20, 50);
    canvas.print("Clean-boot platform selector");

    for (int i = 0; i < 4; ++i) {
      const IQ200Mode mode = entries[i];
      const int y = 76 + i * 51;
      const bool active = i == selected;
      const bool ready = ModeManager::available(mode);
      const uint16_t bg = active ? 0x1C9F : 0x0861;
      const uint16_t border = active ? TFT_CYAN : 0x39E7;
      canvas.fillRoundRect(18, y, 444, 42, 8, bg);
      canvas.drawRoundRect(18, y, 444, 42, 8, border);
      canvas.setTextSize(2);
      canvas.setTextColor(ready ? TFT_WHITE : 0x7BEF, bg);
      canvas.setCursor(34, y + 12);
      canvas.print(label(mode));
      canvas.setTextSize(1);
      canvas.setCursor(350, y + 16);
      canvas.print(ready ? "READY" : "FUTURE");
    }

    canvas.fillRect(0, 286, 480, 34, 0x0841);
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_YELLOW, 0x0841);
    canvas.setCursor(18, 298);
    canvas.print(message);
  }

  void draw() {
    if (!fbReady) {
      drawCanvas(display);
      return;
    }
    drawCanvas(fb);
    display.startWrite();
    fb.pushSprite(0, 0);
    display.endWrite();
  }

  void activate() {
    const IQ200Mode target = entries[selected];
    if (!ModeManager::available(target)) {
      strncpy(message, "Reserved: hardware/driver will be added later", sizeof(message) - 1);
      message[sizeof(message) - 1] = 0;
      messageUntil = millis() + 2500U;
      draw();
      return;
    }
    snprintf(message, sizeof(message), "Starting %s...", label(target));
    draw();
    if (modes.setNext(target)) {
      delay(120);
      ESP.restart();
    }
    strncpy(message, "NVS save failed", sizeof(message) - 1);
    message[sizeof(message) - 1] = 0;
    draw();
  }

public:
  ModeCenterUI(IQ200Display& targetDisplay, ModeManager& manager)
      : display(targetDisplay), modes(manager), fb(&targetDisplay) {}

  void begin() {
    nav.begin();
    confirm.begin();
    fb.setPsram(true);
    fb.setColorDepth(16);
    fbReady = fb.createSprite(480, 320) != nullptr;
    Serial.printf("[MODE_UI] framebuffer=%s bytes=%u\n",
                  fbReady ? "PSRAM_OK" : "DIRECT_FALLBACK",
                  fbReady ? 480U * 320U * 2U : 0U);
    draw();
  }

  void tick() {
    const int delta = nav.delta(false, 2);
    if (delta) {
      selected += delta;
      if (selected < 0) selected = 3;
      if (selected > 3) selected = 0;
      strncpy(message, "Rotate NAV, press NAV or VOL to start", sizeof(message) - 1);
      message[sizeof(message) - 1] = 0;
      draw();
    }
    if (nav.pressed() || confirm.pressed()) activate();
    if (messageUntil && (int32_t)(millis() - messageUntil) >= 0) {
      messageUntil = 0;
      strncpy(message, "Rotate NAV, press NAV or VOL to start", sizeof(message) - 1);
      message[sizeof(message) - 1] = 0;
      draw();
    }
  }
};
