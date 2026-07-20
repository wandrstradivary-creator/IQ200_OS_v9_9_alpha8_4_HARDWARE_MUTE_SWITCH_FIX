# IQ200 OS v9.0.15 Autoplay Runtime Verify

Runtime fix after real v9.0 Core hardware log.

## Fixed
- Updated remaining v8.9/v8.2.5 banners in diagnostics/Web/DB output.
- Unsupported FLAC/OPUS/MP3/OGG/AAC no longer leave play guard busy.
- IQPlayerCore now returns READY after unsupported codec and prints clear decoder status.
- Added `playwav` command to jump to the next indexed WAV track and test the real SD -> PCM16 -> I2S -> PCM5102 path.

## Test
```text
diag
db
play
stop
playwav
media
stop
```
