# IQ200 OS v7.0 RT Audio

Base: v6.9.4 RESOURCE TEST.

Changes:
- Dedicated WAV RT task pinned to Core0 with higher priority.
- Core1 UI no longer pumps audio.
- Player screen does not call audio.begin() while WAV is playing.
- VU is calculated inside the audio path and exposed as atomic left/right levels.
- UI redraw is throttled for the Player screen.
- WAV playback writes complete PCM chunks to I2S.
- No Serial progress spam inside the real-time audio path.

Test commands: `wav`, `play`, `stop`, `player`, `tone`, `status`.
