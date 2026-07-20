# IQ200 OS v9.2-alpha8 PLAYER LOW RESOURCE

## Changes
- Player VU/Peak limited to 8 FPS.
- Progress limited to 2 FPS.
- Health line limited to 0.5 FPS.
- Player change detection limited to 20 Hz.
- Tiny VU changes below 3 units do not trigger LCD writes.
- Event-driven title/state/volume updates preserved.
- FLAC SD exclusive protections from alpha7 preserved.

## Expected result
Lower Core1 and SPI display load during FLAC playback without changing audio decoding.
