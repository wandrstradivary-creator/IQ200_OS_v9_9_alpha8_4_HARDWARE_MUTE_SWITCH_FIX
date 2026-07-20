# IQ200 OS v9.8-alpha38 AUDIO PRIORITY ART GUARD

- Blocks TFT JPEG/PNG decode and RGB565 scaling while MP3/FLAC/WAV RT playback is active.
- Keeps pending artwork generation and renders it when audio becomes idle.
- Prevents manual artwork reload from reading SD during active playback.
- Web artwork cache remains available.
- Audio pipeline, EQ, Resume, VU and library logic are unchanged.
