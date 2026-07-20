# IQ200 OS v9.8-alpha6 — SD Recovery 12 MHz → 8 MHz

## Changes
- SD mount profile is now 12 MHz first, 8 MHz fallback.
- 16 MHz is disabled after long-run CMD13/no-token failures.
- Invalid `File` state no longer bypasses recovery.
- Seek failure and zero-byte read enter remount/reopen recovery.
- Failed partial prefetch is discarded before decoder sees it.
- Recovery closes SD, remounts, reopens the same path and seeks to the last logical byte.
- Governor locks the downshifted 8 MHz frequency until reboot.

## Expected recovery log
```text
[SDM][READ] retry=1 ... freq=12000000
[SDM][RECOVERY] begin ... downshift=1
[SDM][GOV] clock 12000000 -> 8000000 Hz (locked until reboot)
[SDM][RECOVERY] remount freq=8000000 ok=1
[SDM][RECOVERY] stream reopened pos=...
[SDM][RECOVERY] resumed transport ...
```

## Test
```text
sd status
sd stats
play
```
Run a long FLAC/MP3 track and verify `freq=12000000`. If a card fault occurs, verify recovery at `freq=8000000` and playback continuation.
