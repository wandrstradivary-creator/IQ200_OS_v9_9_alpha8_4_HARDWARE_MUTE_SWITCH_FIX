# IQ200 OS v9.3-alpha2.7 SD RECOVERY + PREFETCH

## Added
- 128 KB compressed FLAC prefetch cache in PSRAM.
- Physical SD reads limited to 8 KB chunks.
- Automatic SD remount, stream reopen and compressed byte-position restore.
- Recovery counters: `recover` and `fatal`.

## Changed
- Default SD clock: 16 MHz, fallback 12/8 MHz.
- FLAC decode block: 4096 frames.

## Disabled
- 20 MHz SD profile.
- Direct burst decoding with 8192-frame blocks.
- Immediate fatal stop after the first transient pre-EOF zero read.

## Expected log
```text
[SDM][PREFETCH] cache=128KB physicalChunk=8KB
[SDM][RECOVERY] remount freq=16000000 ok=1
[SDM][RECOVERY] stream reopened pos=.../...
```
