#pragma once
#include <Arduino.h>
#include <LovyanGFX.hpp>
#include "iq200_pins.h"

class IQ200Display : public lgfx::LGFX_Device {
  lgfx::Panel_ILI9488 _panel;
  lgfx::Bus_SPI _bus;
  lgfx::Light_PWM _light;

public:
  IQ200Display() {
    auto b = _bus.config();
    b.spi_host = SPI2_HOST;
    b.spi_mode = 0;
    b.freq_write = 40000000;   // stable fast ILI9488
    b.freq_read  = 16000000;
    b.spi_3wire = false;
    b.use_lock = true;
    b.dma_channel = SPI_DMA_CH_AUTO;
    b.pin_sclk = IQ200_TFT_SCLK;
    b.pin_mosi = IQ200_TFT_MOSI;
    b.pin_miso = IQ200_TFT_MISO;
    b.pin_dc   = IQ200_TFT_DC;
    _bus.config(b);
    _panel.setBus(&_bus);

    auto p = _panel.config();
    p.pin_cs = IQ200_TFT_CS;
    p.pin_rst = IQ200_TFT_RST;
    p.pin_busy = -1;
    p.panel_width = 320;
    p.panel_height = 480;
    p.offset_x = 0;
    p.offset_y = 0;
    p.offset_rotation = 0;
    p.dummy_read_pixel = 8;
    p.dummy_read_bits = 1;
    p.readable = true;
    p.invert = true;
    p.rgb_order = false;
    p.dlen_16bit = false;
    p.bus_shared = true;
    _panel.config(p);

    auto l = _light.config();
    l.pin_bl = IQ200_TFT_BL;
    l.invert = false;
    l.freq = 44100;
    l.pwm_channel = 7;
    _light.config(l);
    _panel.setLight(&_light);

    setPanel(&_panel);
  }

  bool begin() {
    pinMode(IQ200_TFT_BL, OUTPUT);
    digitalWrite(IQ200_TFT_BL, HIGH);
    bool ok = init();
    setRotation(1);
    setBrightness(255);
    return ok;
  }
};
