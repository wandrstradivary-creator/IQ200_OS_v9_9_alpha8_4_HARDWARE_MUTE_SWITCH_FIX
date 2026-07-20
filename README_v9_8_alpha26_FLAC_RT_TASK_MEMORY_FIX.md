# IQ200 OS v9.8-alpha26 — FLAC RT Task Memory Fix

Fixes FLAC startup failure at pipeline step S11.

- helper stereo buffer prefers PSRAM;
- FLAC RT stack reduced from 16384 to 12288 bytes;
- automatic 8192-byte fallback;
- logs total and largest internal heap block;
- alpha25 pipeline diagnostics retained.
