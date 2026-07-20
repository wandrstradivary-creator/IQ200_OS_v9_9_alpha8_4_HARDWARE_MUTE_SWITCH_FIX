# IQ200 OS v9.3-alpha2.5 MAX SD + 64KB PCM

## Added
- Adaptive SD SPI mount profile: 20 MHz -> 16 MHz -> 12 MHz -> 8 MHz.
- 8192-frame FLAC decode block.
- 32 KB PCM buffer + 32 KB stereo conversion buffer (64 KB total).
- Selected SD frequency in the boot log.

## Changed
- FLAC decode chunk increased from 4096 to 8192 frames.
- SD starts at the highest practical SPI rate and falls back automatically if mount fails.

## Disabled
- Fixed 8 MHz-only SD mount.
- 4096-frame FLAC decode profile.

## Preserved
- SD exclusive stream ownership.
- Event Queue/UI Scheduler optimization.
- Boot volume 8%.
- Artwork, Auto Next, segmented VU and Peak Hold.

## Expected log
```text
[SDM] begin ... freq=20000000 ok=1
[SD] mount ... freq=20000000
[FLAC] buffers frames=8192 bytesEach=32768 ready=1
[AUDIO-RT] v9.3-alpha2.5 MAX SD + 64KB PCM ... framesPerRead=8192
```

## Note
20 MHz is the preferred profile, not a guarantee. The firmware automatically falls back to 16/12/8 MHz when the card or wiring cannot mount reliably.
