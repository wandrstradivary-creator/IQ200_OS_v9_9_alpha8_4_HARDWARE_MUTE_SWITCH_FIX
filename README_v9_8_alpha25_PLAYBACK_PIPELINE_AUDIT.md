# IQ200 OS v9.8-alpha25 — Playback Pipeline Audit

Adds numbered FLAC startup diagnostics from stream open through first PCM write.

Expected sequence:

```text
[PIPE] S1 ...
...
[PIPE] S15 PLAYBACK RUNNING
```

AudioEngine now logs the exact ESP-IDF errors returned by I2S install, pin and clock setup. A single stale-driver recovery retry is performed when install returns ESP_ERR_INVALID_STATE.
