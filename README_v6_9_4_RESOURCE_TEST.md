# IQ200 OS v7.0 RT Audio

Test build for continuous WAV crackle while tone is clean.

Changes:
- AudioEngine::writePCMAll(): blocks until the whole PCM chunk is written to I2S. This avoids dropping partial buffers, which can sound like continuous crackle.
- WavPlayerService now counts source WAV bytes only after full PCM write.
- Smaller SRAM-first audio buffers: 4096/8192.
- I2S DMA buffers: 16 x 512.
- Core0 WAV pump window increased to 20 ms.
- Serial progress spam disabled during playback.
- UI VU animation disabled during playback.
- Player redraw throttled to 1 s while audio is playing.

Test commands:
1. `tone` — should remain clean.
2. `play` or `wav` — test /test.wav.
3. If crackle disappears, previous issue was starvation/partial I2S writes/UI load.
4. If crackle remains, check WAV format: must be PCM 16-bit, 44.1 kHz or supported rate, mono/stereo.
