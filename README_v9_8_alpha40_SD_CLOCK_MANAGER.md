# IQ200 OS v9.8-alpha40 SD CLOCK MANAGER

- Web and console selection: 16 / 12 / 10 MHz.
- Commands: `sd speed 16`, `sd speed 12`, `sd speed 10` (aliases `sd 16`, etc.).
- Selection saved in NVS namespace `iq200-sd`.
- Safe runtime remount is rejected while an audio stream is active; stop playback first.
- Boot falls back to 12 MHz and then 10 MHz if the preferred clock cannot mount.
- `/api/status` exposes `sdClockMHz`.
