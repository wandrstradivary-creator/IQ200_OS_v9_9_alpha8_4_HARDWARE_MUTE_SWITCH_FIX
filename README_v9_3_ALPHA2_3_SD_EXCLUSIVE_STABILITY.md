# IQ200 OS v9.3-alpha2.3 — SD EXCLUSIVE STABILITY

## Added
- Exclusive SD stream session counter.
- Background SD operation gate while FLAC stream is active.
- SD diagnostics: `denied` and `sessions`.

## Changed
- Immediate SD helpers now reject background open/exists/mkdir/remove/rename/card-status calls during active audio streaming.
- Stability probe runs only in explicit burn-in mode and never during an active SD stream.

## Disabled
- Background SD metadata/status/write traffic during FLAC playback.
- Periodic `stability.probe` writes during normal operation.

## Preserved
- dr_flac callback transport and tell callback compatibility.
- Artwork load before decoder start.
- Auto Next EOF Guard.
- Player geometry, VU 20 FPS and Peak Hold.

## Expected diagnostics
```text
[SDM] ... denied=0 sessions=1 ...
```
During playback, `sessions` should be at least 1. `denied` may rise when a background service attempts SD access; that request is blocked instead of disturbing audio.
