# IQ200 OS v9.0.15 — State Panic Guard

X10THINK runtime fix after v9.0.3 hardware log.

## Fixed

- SmartResume no longer overwrites a restored FLAC/OPUS/MP3/OGG/AAC resume point with `0%` while those decoders are still not built.
- MediaEngine no longer uses the WAV-only RT mirror to clear non-WAV progress/state.
- Unsupported decoder playback keeps restored progress visible while honestly reporting `decoder not built`.
- `/test.wav` remains a hardware-only `playwav` test and still does not poison real SmartResume.
- Version/banner updated to `9.0.15_AUTOPLAY_RUNTIME_VERIFY`.

## Test

```text
diag
resume
media
play
media
stop
playwav
media
stop
resume
reboot
resume
media
```

Expected: resume path is the real FLAC track, not `/test.wav`; restored progress is not auto-saved as zero simply because FLAC decoder is not built yet.
