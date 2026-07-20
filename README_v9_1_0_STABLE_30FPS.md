# IQ200 OS v9.1.0 STABLE 30 FPS

Clean branch based on the verified v9.0.15 Autoplay Runtime Verify source tree.

## Changes

- Player partial renderer target: 30 FPS (33 ms cadence).
- Only progress, VU, state, volume and changed title are updated at high frequency.
- Full-screen pages remain event-driven to avoid unnecessary ILI9488 SPI traffic.
- Core1 UI polling delay reduced from 2 ms to 1 ms.
- Existing FLAC decoder, Autoplay and track-only Resume are preserved.
- Experimental v9.0.17/v9.0.18 SD bus recovery changes are not included.

## Runtime verification

```text
player
play
diag
media
stop
```

Expected diagnostics while the Player screen is open:

```text
[DIAGPRO] FPS=... target=30 ... partial=... dirty=...
```

Actual FPS can be below 30 when widgets do not change; the renderer intentionally skips identical frames.
