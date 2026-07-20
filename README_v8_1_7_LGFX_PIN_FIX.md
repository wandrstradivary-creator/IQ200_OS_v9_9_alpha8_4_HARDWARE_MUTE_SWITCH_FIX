# IQ200 OS v8.1.7 LGFX PIN FIX

Fixes PlatformIO/LovyanGFX build error where LovyanGFX 1.2.24 compiles esp32p4/Touch_ST7123.cpp and fails on missing ../../utility/pgmspace.h.

Changes:
- platformio.ini pins LovyanGFX to lovyan03/LovyanGFX@1.1.16
- IQ200_VERSION updated to 8.1.7_LGFX_PIN_FIX
- RT Audio unchanged

After extracting, delete `.pio/libdeps/.../LovyanGFX` or run `pio run -t clean` before rebuilding.
