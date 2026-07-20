# IQ200 OS v9.2-alpha15 PROCESS TRACE

Detailed runtime tracing build for process and task diagnostics.

## Default trace

Tracing is enabled at boot and prints one Core0 and one Core1 summary per second.

- `[PROC][C0]`: services, player/decoder, playlist, handoff, scan, event queue.
- `[PROC][C1]`: UI task, render rates, app/screen, events and watchdog ages.
- `[PROC][CMD]`: every serial command and the state in which it arrived.
- `[PROC][EVT+]`: event posted by Core0/service side.
- `[PROC][EVT-]`: event consumed by Core1/UI side.
- `[SDM]`: SDManager lock/open/cross-core statistics every five seconds.

High-frequency WAV/FLAC progress events are intentionally omitted from EVT +/- lines to avoid Serial flooding.

## Commands

```
log
log status
log on
log off
log reset
log rate 250
log rate 1000
log rate 5000
```

The supported summary interval is 250..10000 ms.

## Test recommendation

Use `log rate 500` only for short reproductions. For long playback tests use 1000-5000 ms because intensive Serial output can affect timing.
