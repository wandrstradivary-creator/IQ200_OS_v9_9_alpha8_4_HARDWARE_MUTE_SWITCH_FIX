# IQ200 OS v9.2-alpha13 SEGMENTED VU

- Rectangular segmented VU meter.
- Runtime segment count: 4..24.
- Default: 12 segments per channel.
- Serial commands:
  - `vuseg` — show current count.
  - `vuseg 8` / `vuseg 12` / `vuseg 16` / `vuseg 20` — set count.
- VU refresh: 20 FPS.
- Peak Hold retained.
- Delta rendering updates only changed segments.
