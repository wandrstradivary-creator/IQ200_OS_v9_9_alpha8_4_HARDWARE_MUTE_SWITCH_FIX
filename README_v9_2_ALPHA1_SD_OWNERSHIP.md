# IQ200 OS v9.2-alpha1 — SD Ownership Foundation

## Added

- `SDManager` with a recursive FreeRTOS mutex.
- Full-session `SDManager::Guard` for open/read/write/close sequences.
- SD access diagnostics: lock count, timeout count, cross-core access, open failures and lock wait time.
- `StorageService::mount()` now initializes SD through `SDManager` and protects the boot-file session.

## Important

This alpha establishes the arbitration layer but does **not** yet migrate every direct `SD.*` and `File` operation. Therefore it is a diagnostic/foundation build, not the final SD concurrency fix.

## Test focus

1. Confirm normal boot and SD mount.
2. Capture `[SDM] begin ...` output.
3. Test MP3/FLAC/WAV playback and UI activity.
4. Record any `sdCommand(): no token received` or `Card Failed!` messages.
5. The next commit migrates long-lived file sessions and moves UI-triggered filesystem work to Core0.
