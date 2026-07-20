# IQ200 OS v9.0 CORE — X10THINK

Base: v8.9 Release Candidate.

Goal: start the Enterprise audio-core transition without breaking the stable SD/DB/UI stack.

## Added

- `src/services/IQPlayerCore.h`
- codec-neutral `IQPlayerCore` facade
- single playback route: `play` -> `IQPlayerCore` -> decoder -> PCM -> I2S -> PCM5102
- real local WAV PCM16 path remains active through the proven RT audio task
- honest decoder status for FLAC/OPUS/MP3/OGG/AAC: indexed and selectable, but not faked as playback
- v9 format priority:
  1. FLAC
  2. OPUS
  3. MP3
  4. OGG/OGA
  5. WAV
  6. AAC/M4A

## Preserved from v8.9 RC

- SD boot database
- DatabaseService layout
- ScanService background task
- SmartResume
- Diagnostics Pro
- Stability/Burn-in
- Web UI / OTA Base
- Commercial Polish
- Cyrillic search fix

## Test plan

Serial:

```text
status
db
find цой
play
stop
next
play
media
diag
stability
```

Expected:

- FLAC files are selected according to v9 priority.
- If a FLAC/OPUS/MP3/OGG/AAC file is played, firmware reports `decoder not built` instead of pretending playback works.
- WAV files still play through SD -> WAV PCM16 -> I2S -> PCM5102.
- `diag` should show `audioRtLoops > 0` only during real WAV playback.

## Notes

This is a safe v9 audio-core bridge. The next step is adding the first real compressed decoder, ideally FLAC first, while keeping this same IQPlayerCore API.
