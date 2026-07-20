# IQ200 OS v9.0.15 — Autoplay Engine Integration

## Goal
Integrate the first real non-WAV decoder path into IQPlayerCore without returning to ESP32-audioI2S.

## Added
- `FLACPlayerService.h/.cpp`
- dr_flac backend via `https://github.com/mackron/dr_libs.git`
- FLAC path: SD/FATFS -> dr_flac -> PCM16 stereo -> I2S -> PCM5102
- FLAC RT task pinned to Core0
- FLAC VU/progress/underrun/short-write metrics mirrored into RuntimeState
- WAV hardware test path remains unchanged

## Codec status
- FLAC: real decoder path added
- WAV: real PCM16 SD->I2S path preserved
- OPUS/MP3/OGG/AAC: indexed, guarded, decoder-not-built message remains

## Test
```
diag
find цой
play
media
stop
playwav
media
stop
```

## Notes
This release depends on PlatformIO fetching dr_libs from GitHub. If build fails on `dr_flac.h`, clear `.pio/libdeps` and rebuild with internet access.
