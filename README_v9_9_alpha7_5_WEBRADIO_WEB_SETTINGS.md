# IQ200 OS v9.9-alpha7.5 — WebRadio Web Settings

The WebRadio clean-boot platform now exposes the same useful Web Appearance controls as Local Player:

- TFT theme and brightness
- playback volume
- VU style, segment count, FPS, peak hold and decay
- EQ presets and custom Bass/Mid/Treble

All WebRadio settings are persisted in NVS. Decoder operations remain owned by `webradio_rt`.

Compatibility fix: ESP32-audioI2S 2.0.7 does not provide `getVUlevel()`. VU levels are calculated from its supported post-DSP `audio_process_i2s` PCM callback, without changing the pinned library version.

After boot, open **Appearance** in the Web UI. Local-only SD, navigation and player-layout cards remain hidden in WebRadio mode.
