#pragma once
#include <Arduino.h>
#include <LovyanGFX.hpp>

class StatusBar {
public:
  template<class GFX>
  void draw(GFX& g, const char* left, const char* right) {
    g.fillRect(0, 0, 480, 22, TFT_NAVY);
    g.setTextSize(1);
    g.setTextColor(TFT_WHITE, TFT_NAVY);
    g.setCursor(6, 7);
    g.print(left);
    g.setCursor(350, 7);
    g.print(right);
  }
};
