# IQ200 OS v8.1.8 BUILD STABLE X10THINK

Purpose: stabilize the build after v8.1.x UI/library additions.

Changes:
- Version banner synchronized to v8.1.8.
- `platformio.ini` pins LovyanGFX to `lovyan03/LovyanGFX@1.1.16`.
- IQ200_VERSION updated to `8.1.8_BUILD_STABLE`.
- No RT Audio changes.

Important build note:
If PlatformIO still compiles LovyanGFX 1.2.24 or `esp32p4/Touch_ST7123.cpp`, delete `.pio` or at least:

```text
.pio/libdeps/esp32s3-n16r8-iq200-os-v71-rt-audio-stabilizer/LovyanGFX
```

Then rebuild.
