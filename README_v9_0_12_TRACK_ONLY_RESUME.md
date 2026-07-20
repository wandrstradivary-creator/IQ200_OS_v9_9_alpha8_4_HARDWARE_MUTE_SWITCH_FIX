# IQ200 OS v9.0.15 — Autoplay Engine

Requested policy change: SmartResume stores only the selected/current track target.
Playback position is no longer restored or treated as a resume target.

## Changes

- ResumeEngine writes `PLAYED=0` and `PROGRESS=0` for compatibility.
- ResumeEngine ignores old `PLAYED/PROGRESS` values when loading old resume files.
- SmartResume logs `mode=TRACK_ONLY`.
- Autosave is no longer triggered by progress/played byte changes.
- Resume still keeps track path/title/codec, volume, repeat, and playlist index/count.
- `playwav` remains a hardware test and does not replace normal library resume.

## Expected serial behavior

```text
[SMARTRESUME] restored track-only: path=/Music/... volume=6 repeat=2
[RESUME] PLAYED=0
[RESUME] PROGRESS=0
[SMARTRESUME] enabled=1 mode=TRACK_ONLY ...
```

## Test

```text
diag
resume
play
media
resume
stop
resume
reboot
resume
media
```

After reboot the same track should be selected, but progress should always be 0%.
