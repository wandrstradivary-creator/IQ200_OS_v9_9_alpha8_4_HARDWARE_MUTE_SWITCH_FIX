# IQ200 OS v8.2.2 — ScanService

X10THINK step after v8.2.1 Build Verify.

## Main changes
- Added `src/services/ScanService.h`.
- Full SD media database rebuild now runs in a dedicated FreeRTOS task.
- Core0 worker only schedules scan requests and keeps ticking services/watchdog.
- Core1 UI/encoder task remains responsive during large SD scans.
- Scan progress, files, tracks, dirs, MP3/FLAC/WAV counters still mirror to `RuntimeState`.
- Audio is stopped safely before scan to reduce SD contention.
- Library indexes are rebuilt after scan inside the scan task.

## Commands to test
```text
status
scan
status
db
artists
albums
play
```

Expected scan logs start with `[SCANSVC]`.
