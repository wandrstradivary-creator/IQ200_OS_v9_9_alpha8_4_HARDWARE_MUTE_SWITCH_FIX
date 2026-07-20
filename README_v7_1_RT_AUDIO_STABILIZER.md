# IQ200 OS v7.1 RT Audio Stabilizer

Base: v7.0 RT Audio.

Changes:
- Audio RT task remains pinned to Core0.
- Task stack increased to 12288 bytes.
- Audio task priority increased to 5.
- RT loop Serial spam removed; only start/stop summaries remain.
- Added runtime diagnostics: health, underruns, short writes, RT loops, last chunk size, stack high-water.
- Player footer and progress panel show H/U/W diagnostics.
- VU stays enabled but remains lightweight: audio task computes two bytes, UI draws small bars.

Test goal:
- Verify WAV remains clean with VU enabled.
- Watch H/U/W during playback. H should stay near 100, U=0, W=0.

Not locally compiled in this environment. Open in PlatformIO and build for ESP32-S3 hardware target.
