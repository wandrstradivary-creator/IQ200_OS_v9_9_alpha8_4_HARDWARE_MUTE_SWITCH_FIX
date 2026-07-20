# IQ200 OS v9.9-alpha7.4 — WebRadio VU

This build adds a stereo L/R VU display to the isolated WebRadio platform.

- Source in alpha7.5: live PCM levels from the ESP32-audioI2S 2.0.7 `audio_process_i2s` callback.
- Sampling: 40 Hz inside the existing `webradio_rt` owner task.
- Rendering: 20 FPS using a dedicated 440x48 RGB565 PSRAM sprite.
- Display: 24 segments per channel, green/yellow/red zones and peak hold.
- Safety: no decoder call from UI or Web tasks, no dynamic allocation per frame, and no full-screen redraw for VU animation.
- Diagnostics: `/api/status` reports `radioVuL`, `radioVuR` and `radioVuTicks`.

Expected serial line after entering WebRadio:

```text
[WEBRADIO_UI] vu=PSRAM_OK bytes=42240 fps=20
```

Hardware check:

1. Enter WebRadio and start a working MP3/AAC station.
2. Confirm both L/R scales move and volume remains responsive.
3. Press Stop; both scales must fall to zero.
4. Change stations repeatedly and confirm no stale peak remains.
5. Keep playback running for at least 30 minutes while polling `/api/status`; Web UI and audio must remain responsive.
