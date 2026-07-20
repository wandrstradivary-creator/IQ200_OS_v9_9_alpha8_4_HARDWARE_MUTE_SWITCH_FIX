# IQ200 OS v7.3 MEDIA FRAMEWORK X10THINK

Base: v7.2.4 Alias Help Stable.

Added:
- Media Framework foundation (`MediaFramework.h`)
- Codec-neutral `IDecoder` interface for future WAV/MP3/FLAC/Radio
- `MediaTrack` and codec detection by path
- Playlist control bridge through RuntimeState
- Serial commands: `pl`, `pladd`, `plclear`, `plnext`/`next`, `plprev`/`prev`
- `play` now opens the selected `runtimeState.mediaPath` instead of hardcoded `/test.wav`
- Startup version updated to v7.3

Kept stable:
- RT WAV audio path from v7.2.x
- play guard
- alias/help fixes
- no changes to DMA/I2S timing

Test order:
```text
h
pl
pladd
plnext
play
health
stop
```
