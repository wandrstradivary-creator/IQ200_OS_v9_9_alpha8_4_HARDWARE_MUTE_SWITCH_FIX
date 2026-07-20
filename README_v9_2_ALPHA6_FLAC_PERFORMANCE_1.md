# IQ200 OS v9.2-alpha6.1 FLAC COMPILE FIX

Focused FLAC playback optimization only.

## Changes
- FLAC decode chunk: 1024 -> 4096 PCM frames.
- Hot PCM and mono-to-stereo buffers prefer internal RAM.
- PSRAM remains a safe allocation fallback.
- Forced task yield reduced from every block to every eighth block.
- Added startup diagnostics for buffer size and memory placement.

## Expected log
```text
[FLAC] buffers frames=4096 bytesEach=16384 pcmInternal=1 stereoInternal=1
[AUDIO-RT] v9.2-alpha6.1 FLAC compile fix task core=0 framesPerRead=4096
```

## Test
Play several FLAC files, including 44.1/48/96 kHz if available. Check for clicks,
shortWrites, early EOF and SD errors. Auto Next and player-screen behavior are retained.
