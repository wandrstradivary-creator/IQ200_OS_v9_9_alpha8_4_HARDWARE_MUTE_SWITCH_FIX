# IQ200 OS v9.0.15 — Autoplay Engine

Based on v9.0.7 Player UI + Volume Fix.

Fixes:
- stops the active FLAC/WAV decoder before any new play request;
- prevents FLAC from continuing when `next/prev` lands on unsupported MP3/OPUS/OGG/AAC;
- unsupported decoders return UI to READY and clear VU/progress task state;
- SmartResume no longer saves unsupported idle MP3/OPUS/OGG/AAC as the resume target;
- RT completion log is codec-neutral instead of always saying WAV;
- version/env updated to v9.0.15.

Test:
```
diag
play
prev
media
resume
playwav
media
stop
resume
```
