#pragma once
#include <Arduino.h>
#include <LovyanGFX.hpp>

class Popup {
public:
  template<class GFX>
  void draw(GFX& g, const char* msg) {
    g.fillRoundRect(80, 116, 320, 80, 12, TFT_NAVY);
    g.drawRoundRect(80, 116, 320, 80, 12, TFT_CYAN);
    g.setTextSize(2);
    g.setTextColor(TFT_WHITE, TFT_NAVY);
    g.setCursor(105, 148);
    g.print(msg);
  }
};
