# IQ200 OS v9.0.15 — Autoplay Engine

Fixes from real hardware feedback:

- Player screen now shows the current track title from `RuntimeState::mediaTitle` instead of hardcoded `test.wav`.
- Player screen progress now follows codec-neutral `mediaProgress`, so FLAC/WAV playback can update the screen.
- Partial renderer now updates progress, title, state, VU, volume, and health line.
- Volume encoder writes both `AudioEngine::setVolume()` and `RuntimeState::volumePercent`.
- `AudioEngine` now applies software volume scaling to PCM16 samples before I2S output.
- WAV and FLAC RT paths keep using SD → decoder/PCM → I2S → PCM5102.

Test commands:

```text
diag
player
play
media
stop
playwav
player
media
stop
```

Encoder test:

```text
player
rotate volume encoder
```

Expected:

- Title visible on Player screen.
- Progress bar moves during WAV/FLAC playback.
- Volume number changes on screen.
- Audio level changes when rotating volume encoder.
