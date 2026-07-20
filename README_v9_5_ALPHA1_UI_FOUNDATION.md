# IQ200 OS v9.5-alpha1 — UI FOUNDATION

## Added

- `src/ui/PlayerLayout.h` as the single source of Player geometry.
- UI widget profiling for full frame, title, state, volume, VU, progress, and artwork.
- Commands:
  - `ui`
  - `ui status`
  - `ui reset`

## Changed

- Player static layout and all high-frequency partial draws now use `PlayerLayout` rectangles.
- Existing dirty renderer remains active; each dynamic region is timed independently.
- Version banner and help text updated.

## Unchanged

- FLAC decoder and RT task.
- SD 16 MHz, 128 KB prefetch, 8 KB physical reads, and recovery.
- Auto Next and handoff FSM.
- Theme Engine, VU PRO/Line, Black Box, and performance dashboard.

## Test

```text
player
ui reset
play
ui
next
ui
```

Expected output:

```text
========== UI PROFILER ==========
screen=... fps=... full=... partial=... dirty=...
full       draws=... avg=...us max=...us
vu         draws=... avg=...us max=...us
progress   draws=... avg=...us max=...us
artwork    draws=... avg=...us max=...us
=================================
```
