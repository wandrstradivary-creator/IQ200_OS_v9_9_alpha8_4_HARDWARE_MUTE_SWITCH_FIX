# IQ200 OS v9.8-alpha11 SD RECOVERY BRIDGE

## Goal
Keep normal audio streaming at 12 MHz. Use 8 MHz only as a short SD recovery bridge.

## Recovery sequence
1. Retry the failed physical read at 12 MHz.
2. Stop exposing partial prefetch data to the decoder.
3. Remount SD briefly at 8 MHz.
4. Restore SD to 12 MHz.
5. Reopen the active file and seek to the last logical position.
6. Refill prefetch at 12 MHz.
7. Resume decoder transport.

8 MHz is never used for sustained playback.

## Expected log
```text
[SDM][RECOVERY] begin ... bridge=8M->12M
[SDM][BRIDGE] enter 8 MHz recovery bridge
[SDM][RECOVERY] remount 12000000 -> 8000000 Hz ok=1
[SDM][BRIDGE] card responded at 8 MHz; restoring 12 MHz
[SDM][RECOVERY] remount 8000000 -> 12000000 Hz ok=1
[SDM][BRIDGE] working clock restored to 12 MHz
[SDM][RECOVERY] stream reopened ... freq=12000000
```
