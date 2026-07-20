# IQ200 OS v9.0.15 — Resume / State Fix

Based on v9.0.2 Core RT Polish.

Fixes from real hardware log:

- Ignore old `/test.wav` SmartResume entries at boot.
- Do not let hardware test WAV poison normal library resume.
- RuntimeState defaults no longer boot as `/test.wav`.
- Version strings updated to v9.0.15.
- Unsupported decoders remain honest: FLAC/OPUS/MP3/OGG/AAC are indexed but not yet decoded.
- Real WAV path remains active: SD -> PCM16 -> I2S -> PCM5102.

Test:

```text
diag
resume
media
play
media
stop
playwav
media
stop
resume
reboot
resume
media
```
