# IQ200 OS v9.0.15 Autoplay Runtime Verify

Hardware-test polish based on v9.0.1 logs.

Changes:
- keeps real WAV SD -> PCM16 -> I2S -> PCM5102 path;
- updates all visible v9.0.1 banners to v9.0.15;
- Audio RT banner now reports v9.0.15;
- unsupported FLAC/OPUS/MP3/OGG/AAC puts media state into ERROR, not LOADING/busy;
- natural WAV EOF keeps progress at 100% until explicit stop;
- playwav /test.wav no longer overwrites normal Smart Resume target.

Test commands:

```text
diag
db
play
media
stop
playwav
media
stop
resume
```
