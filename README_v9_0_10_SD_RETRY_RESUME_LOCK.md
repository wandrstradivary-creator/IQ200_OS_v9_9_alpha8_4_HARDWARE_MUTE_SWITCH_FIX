# IQ200 OS v9.0.15 — Autoplay Engine

Focus: runtime stability for long FLAC playback on ESP32-S3 + SD + PCM5102.

Changes:
- FLAC read path now retries short SD read interruptions before declaring SDERR.
- SD read failure is not treated as normal EOF and does not become 100% progress.
- SmartAutoplay Engine: playlist navigation (`next`/`prev`) no longer overwrites resume unless real playback is active.
- Restored non-zero resume progress is protected until decoder progress passes the restored point.
- `next`/`prev` stop active FLAC/WAV decoder before changing selected track.
- Version/env updated to v9.0.15.

Test:
```
diag
play
media
resume
prev
media
playwav
media
stop
resume
```
