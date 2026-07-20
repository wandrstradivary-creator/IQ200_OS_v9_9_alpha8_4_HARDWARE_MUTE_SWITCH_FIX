# IQ200 OS v9.8-alpha27 LARGE ARTWORK SUPPORT

- Accepts JPEG/PNG artwork up to 3 MiB.
- Files larger than 512 KiB are allocated only in PSRAM.
- SD reads use 8 KiB chunks with watchdog servicing and cooperative yields.
- Existing fit-center rendering is preserved.
- FLAC RT task memory fix from alpha26 is unchanged.

Expected log for a large cover:

```text
[ART][LARGE] load begin bytes=1595312 ...
[ART][LARGE] load OK bytes=1595312 format=JPEG ...
[ART][FIT] src=... box=190x180 ... ok=1
```
