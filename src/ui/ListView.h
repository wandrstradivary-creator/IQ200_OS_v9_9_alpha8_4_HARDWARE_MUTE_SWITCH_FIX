#pragma once
#include <Arduino.h>
#include <LovyanGFX.hpp>

class ListView {
public:
  template<class GFX>
  void row(GFX& g, int x, int y, int w, const char* text, bool selected) {
    uint16_t bg = selected ? TFT_DARKGREEN : TFT_BLACK;
    uint16_t fg = selected ? TFT_WHITE : TFT_LIGHTGREY;
    g.fillRoundRect(x, y, w, 24, 5, bg);
    if (selected) g.drawRoundRect(x, y, w, 24, 5, TFT_GREEN);
    g.setTextSize(1);
    g.setTextColor(fg, bg);
    g.setCursor(x + 8, y + 8);
    g.print(text);
  }
};
