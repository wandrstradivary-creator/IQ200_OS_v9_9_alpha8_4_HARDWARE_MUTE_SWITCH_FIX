# IQ200 OS v7.4.1.1 SCAN RESET X10THINK

Added real scan-screen telemetry:
- Core1-driven live timer HH:MM:SS
- files/sec
- directory count
- MP3 / FLAC / WAV counters
- current scanned path
- ETA estimate from progress
- scan state remains locked until scan completes

RT Audio path unchanged.


## v7.4.1 SCAN RESET
- scan/rescan always starts from 00:00:00.
- Files, folders, MP3/FLAC/WAV counters reset to 0 before scan.
- ETA shows --:-- until enough progress data exists.
- Current path starts as Waiting..., not stale previous path.
