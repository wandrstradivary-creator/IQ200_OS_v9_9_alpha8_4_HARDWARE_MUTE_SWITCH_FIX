# IQ200 OS v9.0.15 — State / Panic Guard

Focused runtime fix after v9.0.4 hardware test.

## Fixed

- Unsupported FLAC/OPUS/MP3/OGG/AAC no longer leave the UI in `LOADING`.
- `play` for unsupported formats returns to `READY` immediately.
- `resume` printing explicitly closes `resume.dat` after reading.
- Guarded Core0 state after unsupported decoder/open failure.
- Kept real WAV path: SD -> PCM16 -> I2S -> PCM5102.

## Test

```text
diag
resume
media
play
media
resume
playwav
media
stop
resume
reboot
resume
media
```
