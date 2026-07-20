# IQ200 OS v9.9-alpha7.6 — WebRadio VU 2.0.7 Fix

This build fixes a zero/stationary WebRadio VU using the exact callback API shipped by the pinned `esphome/ESP32-audioI2S 2.0.7` package:

```cpp
void audio_process_i2s(uint32_t* sample, bool* continueI2S)
```

The 32-bit post-DSP frame contains signed 16-bit left and right samples. Peak levels are accumulated without allocation and mirrored to the TFT at the configured VU rate. `continueI2S` is explicitly set to `true`, preserving normal audio output.
