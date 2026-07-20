# IQ200 OS v9.0.15 — Autoplay Engine

Runtime fix based on v9.0.8 hardware logs.

## Fixes
- FLAC SD/FAT read failure is no longer treated as normal EOF.
- `Card Failed cmd: 0x0d` / early FLAC stop becomes `MEDIA_STATE_ERROR`, not `STOPPED 100%`.
- SmartResume does not save bogus `100%` or reset-to-0 during SD/read error recovery.
- Last good progress is preserved when decoder aborts early.
- RT log now prints `state=SDERR`, `state=EOF`, or `state=STOP`.
- WAV and FLAC normal EOF paths are preserved.

## Test
```text
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

If SD warnings still appear repeatedly, test another SD card or lower SPI clock next; this build prevents state corruption but cannot repair a physical/card timing issue.
