# IQ200 OS v9.1.1 — Next Track Engine

Base: v9.1.0 Stable 30 FPS.

## Implemented
- Atomic next/prev handoff on Core0.
- Stop-and-wait for active WAV/FLAC FreeRTOS decoder task (1500 ms guard).
- New track is selected only after the previous decoder task exits.
- If playback was active, next/prev automatically starts the selected track.
- If playback was stopped, next/prev only changes the selection.
- SmartResume is locked during handoff and cannot save a transient path.
- Handoff diagnostics: state/count/timeouts/last timestamp.
- 30 FPS partial Player UI from v9.1.0 retained.

## Expected log
```text
[HANDOFF] NEXT: stopping current decoder
[AUDIO-RT] FLAC task stopped state=STOP ...
[HANDOFF] NEXT selected: /Music/... (2/280) autoplay=1
[IQPLAYER] play request [FLAC] /Music/...
[HANDOFF] start OK codec=FLAC path=/Music/...
```

## Test
```text
player
play
next
media
next
prev
resume
diag
```
