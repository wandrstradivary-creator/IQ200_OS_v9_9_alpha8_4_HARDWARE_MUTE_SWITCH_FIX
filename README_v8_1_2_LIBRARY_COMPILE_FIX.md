# IQ200 OS v8.1.2 LIBRARY COMPILE FIX X10THINK

Fixes PlatformIO compile errors from v8.1.1 Library UI.

Changes:
- Added dbMp3Count / dbFlacCount / dbWavCount to RuntimeState.
- Boot media.meta load now stores MP3/FLAC/WAV counters.
- Library UI falls back to scan counters if DB counters are empty.
- statusLine() replaced with statusBar().
- Version banner updated to v8.1.2.

RT Audio unchanged.
