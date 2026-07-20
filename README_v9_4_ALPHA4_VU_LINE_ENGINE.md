# IQ200 OS v9.4-alpha4 VU LINE ENGINE

## Added

- New lightweight `line` VU style.
- Command: `vu style line`.
- One-pixel vertical line per active VU segment using `drawFastVLine()`.
- Theme-aware low/mid/high and peak colors.
- NVS persistence through the existing VU settings service.

## Existing commands

```text
vu status
vu style rect
vu style dot
vu style thin
vu style line
vuseg 4..24
vu fps 10..30
vu peak on/off
vu hold 50..1500
vu decay 1..10
```

## Frozen subsystems

No changes were made to FLAC, SD prefetch/recovery, Auto Next, I2S, Black Box, or Theme Engine.
