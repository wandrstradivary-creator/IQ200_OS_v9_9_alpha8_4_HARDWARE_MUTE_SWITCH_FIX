#pragma once
#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <Preferences.h>

struct IQTheme {
  const char* name;
  uint16_t bg;
  uint16_t panel;
  uint16_t footer;
  uint16_t card;
  uint16_t border;
  uint16_t text;
  uint16_t dim;
  uint16_t accent;
  uint16_t progress;
  uint16_t vuLow;
  uint16_t vuMid;
  uint16_t vuHigh;
  uint16_t peak;
  uint16_t button;
  uint16_t buttonActive;
  uint16_t ok;
  uint16_t warn;
  uint16_t error;
};

class ThemeManager {
  IQTheme theme{};
  uint8_t active = 0;

  static IQTheme make(uint8_t i) {
    // Theme Pack 1.0. Existing NVS indices 1=amber, 2=matrix,
    // 3=blue and 4=light are intentionally preserved.
    constexpr uint16_t GOLD = 0xFEA0;
    switch (i) {
      case 1: // Amber Vintage (legacy amber index)
        return {"amber", 0x0000, 0x2100, 0x2100, 0x1080, 0x8C40,
                0xFFE0, 0xC618, 0xFD20, 0xFC00,
                0xFBE0, 0xFD20, 0xFFE0, GOLD,
                0x2100, 0x8C40, 0xFD20, 0xFFE0, 0xF800};
      case 2: // Matrix (legacy matrix index)
        return {"matrix", 0x0000, 0x0200, 0x0200, 0x0100, 0x03E0,
                0x07E0, 0x03E0, 0x07E0, 0x07E0,
                0x0320, 0x07E0, 0xFFE0, GOLD,
                0x0200, 0x03E0, 0x07E0, 0xFFE0, 0xF800};
      case 3: // Blue Pro (legacy blue index)
        return {"bluepro", 0x0006, 0x0010, 0x0010, 0x0821, 0x229F,
                0xFFFF, 0xBDF7, 0x07FF, 0x049F,
                0x07FF, 0x001F, 0xFFE0, GOLD,
                0x0010, 0x019F, 0x07E0, 0xFFE0, 0xF800};
      case 4: // Ice White (legacy light index)
        return {"ice", 0xEF7D, 0xD69A, 0xD69A, 0xFFFF, 0x9CF3,
                0x0000, 0x528A, 0x041F, 0x049F,
                0x041F, 0x07FF, 0xFD20, GOLD,
                0xD69A, 0xA69F, 0x05E0, 0xFD20, 0xF800};
      case 5: // Emerald Studio
        return {"emerald", 0x0000, 0x10A2, 0x10A2, 0x0841, 0x2C0B,
                0xFFFF, 0x9CD3, 0x07B0, 0x05E0,
                0x07E0, 0xAFE5, 0xFFE0, GOLD,
                0x10A2, 0x05E0, 0x07E0, 0xFFE0, 0xF800};
      case 6: // Purple Neon
        return {"purple", 0x0000, 0x180C, 0x180C, 0x1008, 0x781F,
                0xFFFF, 0xB5D6, 0xA81F, 0x801F,
                0x801F, 0xF81F, 0xFFE0, GOLD,
                0x180C, 0x681F, 0x07E0, 0xFFE0, 0xF800};
      case 7: // Carbon Red
        return {"carbon", 0x0821, 0x1082, 0x1082, 0x18C3, 0x6000,
                0xFFFF, 0xA514, 0xF800, 0xD800,
                0xF800, 0xFD20, 0xFFE0, GOLD,
                0x1082, 0x7000, 0x07E0, 0xFFE0, 0xFFFF};
      default: // OLED Black (legacy dark index)
        return {"oled", 0x0000, 0x0000, 0x0000, 0x0000, 0x4208,
                0xFFFF, 0x8410, 0xFFFF, 0xFFFF,
                0xC618, 0xFFFF, 0xFFE0, GOLD,
                0x0000, 0x4208, 0x07E0, 0xFFE0, 0xF800};
    }
  }

public:
  void begin() {
    Preferences p;
    p.begin("iq200-ui", true);
    active = p.getUChar("theme", 3);
    p.end();
    if (active > 7) active = 0;
    theme = make(active);
  }

  const IQTheme& get() const { return theme; }
  const char* name() const { return theme.name ? theme.name : "dark"; }
  uint8_t index() const { return active; }

  bool set(const String& requested, bool persist = true) {
    String n = requested;
    n.trim(); n.toLowerCase();
    uint8_t next = 255;
    if (n == "oled" || n == "black" || n == "mono" || n == "dark" || n == "0") next = 0;
    else if (n == "amber" || n == "vintage" || n == "1") next = 1;
    else if (n == "matrix" || n == "2") next = 2;
    else if (n == "bluepro" || n == "blue" || n == "3") next = 3;
    else if (n == "ice" || n == "light" || n == "4") next = 4;
    else if (n == "emerald" || n == "green" || n == "studio" || n == "5") next = 5;
    else if (n == "purple" || n == "neon" || n == "6") next = 6;
    else if (n == "carbon" || n == "red" || n == "7") next = 7;
    if (next == 255) return false;
    active = next;
    theme = make(active);
    if (persist) {
      Preferences p;
      p.begin("iq200-ui", false);
      p.putUChar("theme", active);
      p.end();
    }
    return true;
  }

  void printList(Print& out) const {
    out.println("Themes: bluepro, emerald, amber, purple, ice, oled, carbon, matrix");
    out.printf("Active: %s\n", name());
  }
};
