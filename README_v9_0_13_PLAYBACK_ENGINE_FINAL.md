# IQ200 OS v9.0.15 — Autoplay Engine

Stabilization release after v9.0.12 Track Only Resume.

## Focus

- Track-only SmartResume remains: track/index/volume/repeat only.
- Playback state machine names: IDLE / LOADING / PLAYING / STOPPED / EOF / SDERR.
- Playback diagnostics: ring fill, cache hit %, read-ahead bytes, decoder load, SD retries/errors.
- RT progress events use codec-neutral media progress, not WAV-only counters.
- Resume saves only from trusted active playback or explicit rsave; stop/prev/next/EOF do not poison resume.

## Real playback paths

- FLAC: SD -> dr_flac -> PCM16 stereo -> I2S -> PCM5102.
- WAV: SD -> PCM16 -> I2S -> PCM5102.
- MP3/OPUS/OGG/AAC: indexed/guarded, decoder not enabled yet.

## Test

```text
diag
play
media
diag
prev
resume
playwav
media
stop
resume
```

Expected diag now includes:

```text
[DIAGPRO] Player state=PLAYING ring=.. cache=.. readAhead=.. decoderLoad=.. sdRetry=.. sdErr=..
```
