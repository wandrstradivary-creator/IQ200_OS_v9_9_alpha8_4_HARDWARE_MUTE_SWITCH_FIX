# IQ200 OS v9.4-alpha2 — Theme Engine

## Added
- Runtime Theme Engine independent from Audio Core.
- Five built-in themes: `dark`, `amber`, `matrix`, `blue`, `light`.
- Instant switching without reboot.
- Selected theme persisted in ESP32 NVS (`Preferences`).
- Player, common panels, progress, VU, peak, buttons, borders and text use the active palette.

## Commands
```
theme list
theme status
theme dark
theme amber
theme matrix
theme blue
theme light
```

## Boot log
```
[THEME] loaded=dark
```

## Frozen / unchanged
- FLAC decoder and RT task
- SD 16 MHz + 128 KB compressed prefetch
- SD recovery
- Auto Next FSM
- Black Box and performance dashboard
- Boot volume 8%
