# IQ200 OS v9.4-alpha1 — Black Box + Performance Dashboard

Base: v9.3-alpha2.7 SD Recovery + 128 KB compressed prefetch.

## Added

- `BlackBoxService` with a 512-entry circular buffer.
- PSRAM allocation with DRAM fallback.
- Critical event recording from both CPU cores.
- Command history recording.
- Commands:
  - `bb on`
  - `bb off`
  - `bb status`
  - `bb clear`
  - `bb dump`
  - `bb dump <count>`
  - `perf`
- Compact performance report with Player, Audio, memory, tasks, Event Queue,
  Black Box and SDManager statistics.

## Black Box policy

Recorded:

- start/stop/error events;
- SD/index/audio service events;
- Auto Next monitor events;
- user Serial commands.

Not recorded:

- high-rate `EVT_WAV_PROGRESS` events;
- VU samples;
- individual SD reads;
- every UI frame.

This keeps the last important history without affecting real-time playback.

## Retained unchanged

- SD default 16 MHz with 12/8 MHz fallback;
- 128 KB compressed prefetch in PSRAM;
- 8 KB physical SD reads;
- SD stream recovery;
- FLAC decode frames = 4096;
- boot volume = 8%;
- Auto Next EOF handoff;
- Artwork cache;
- VU and Player geometry.

## Test

After flashing:

```text
bb status
perf
play
next
bb dump 64
```

Expected boot line:

```text
[BB] init=1 storage=PSRAM capacity=512
```
