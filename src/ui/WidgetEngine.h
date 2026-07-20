#pragma once
#include <Arduino.h>
#include <LovyanGFX.hpp>

class WidgetEngine {
public:
  template<class GFX>
  void progress(GFX& g, int x, int y, int w, int h, int value, uint16_t fg, uint16_t bg) {
    if (value < 0) value = 0;
    if (value > 100) value = 100;
    g.drawRoundRect(x, y, w, h, 4, fg);
    g.fillRoundRect(x + 2, y + 2, (w - 4) * value / 100, h - 4, 3, fg);
    g.fillRect(x + 2 + (w - 4) * value / 100, y + 2, (w - 4) - ((w - 4) * value / 100), h - 4, bg);
  }

  template<class GFX>
  void card(GFX& g, int x, int y, int w, int h, uint16_t border, uint16_t fill) {
    g.fillRoundRect(x, y, w, h, 8, fill);
    g.drawRoundRect(x, y, w, h, 8, border);
  }
};
