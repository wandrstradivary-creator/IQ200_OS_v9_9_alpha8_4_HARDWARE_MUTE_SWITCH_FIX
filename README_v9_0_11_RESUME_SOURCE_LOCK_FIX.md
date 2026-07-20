# IQ200 OS v9.0.15 — Autoplay Engine

Fixes validated after v9.0.10 logs:

- `prev/next/stop` no longer convert an intentional decoder stop into fake `RT finished 100%`.
- SmartResume is no longer overwritten by playlist navigation or playwav hardware tests.
- `playwav` remains a PCM5102/I2S hardware test and does not replace normal library resume.
- FLAC/WAV RT playback path from v9.0.10 is preserved.
- SDERR/EOF guard from v9.0.9 is preserved.

Test:

```text
diag
play
media
resume
prev
media
resume
playwav
media
stop
resume
```

Expected:

- `prev` prints `RT stopped by control/handoff`, not `RT finished 100%`.
- Resume path remains the last real FLAC playback path unless a real non-test playback advances it.
- `playwav` does not overwrite resume with `/file_example_WAV_*.wav`.
