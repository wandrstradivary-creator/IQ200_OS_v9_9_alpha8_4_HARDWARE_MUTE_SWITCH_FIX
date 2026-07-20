# IQ200 OS v9.5-alpha2.1 — Progress Widget 2.1

Changes are limited to the UI layer.

- Progress percentage text redraws only when the visible integer percentage changes.
- The progress bar remains delta-only: only newly filled or cleared pixels are touched.
- `ui` / `ui status` now reports the real UI update rate from full + partial renderer frames instead of framebuffer-only pushes.
- Audio Core, FLAC, SD prefetch/recovery, Auto Next, Theme Engine and VU are unchanged.

Expected improvement: progress average should drop below the previous 1.7–1.8 ms range in steady playback.
