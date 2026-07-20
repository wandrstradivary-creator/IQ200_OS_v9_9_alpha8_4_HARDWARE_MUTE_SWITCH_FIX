# IQ200 OS v9.2-alpha2 DEFERRED SCAN

## Purpose
Prevent a full SD media database scan from interrupting active audio playback.

## Behavior
- `scan`, `rescan`, or an update fallback requested during playback is queued.
- Audio continues without ScanService calling `stop()`.
- The queued full scan starts automatically once playback and decoder tasks are idle.
- Serial diagnostics:
  - `[SCANSVC] deferred: playback active; use stop to start queued scan`
  - `[SCANSVC] playback idle: starting deferred SD scan`

## Test
1. Start FLAC/MP3/WAV playback.
2. Run `scan`. Audio must continue.
3. Run `stop`. The deferred scan must start automatically.
4. Confirm there are no `sdCommand(): no token received` or `Card Failed!` messages.
