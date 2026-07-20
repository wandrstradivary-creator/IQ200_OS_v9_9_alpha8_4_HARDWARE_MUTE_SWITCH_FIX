# IQ200 OS v9.8-alpha24 — FLAC Transport Audit

## Fixed / added
- Validates the real `fLaC` stream marker before decoder initialization.
- Supports ID3 or junk prefixes up to 64 KB by exposing a virtual FLAC stream base.
- Corrects START/CURRENT seek and tell coordinates relative to that base.
- Adds callback telemetry for read, seek, tell and byte counts.
- Distinguishes invalid/mislabeled FLAC files from SD transport failures.
- Keeps SD Governor, 12 MHz playback, recovery bridge, UI and Web unchanged.

## Expected logs
```text
[FLAC][AUDIT] transport ready size=... base=0 freq=12000000 path=...
[FLAC][AUDIT] decoder OK rate=44100 ch=2 frames=...
```
For a prefixed file, `base` may be non-zero. If the file is damaged or not FLAC, the first bytes are printed.
