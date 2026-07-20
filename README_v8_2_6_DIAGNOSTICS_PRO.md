# IQ200 OS v8.2.6 — Diagnostics Pro

## Changes
- Added Diagnostics Pro serial report via `diag`.
- Shows CPU loop counters, heap, PSRAM, stack high-water marks, SD latency, FPS, renderer counters, audio underruns/short writes, event queues, and DB stats.
- Fixed legacy DB index path normalization after v8.2.4 DatabaseService migration.
- Fixed first-boot missing `/iq200/db/queue/queue.db` VFS warning by checking `SD.exists()` before opening.
- Updated banners/version to `8.2.6_DIAGNOSTICS_PRO`.

## Important runtime fix
If an old `media.idx` still references `/iq200/db/media_000.db`, MediaDatabase now maps it to `/iq200/db/media/media_000.db` before opening.
