# IQ200 OS v7.2 MEDIA FOUNDATION X10THINK

Base: v7.1.4 UI Audio Health.

Added:
- `src/services/MediaEngine.h` codec-neutral facade.
- `MediaInfo`, `MediaCodec`, `MediaState` foundation.
- Runtime media mirror fields independent from WAV naming.
- `media` / `minfo` screen now shows codec, state, track, compact format, BUF/UDR/SW.
- `mplay` alias added; current backend still uses stable RT WAV path.
- Version banner updated to v7.2.

Notes:
- WAV RT playback path is intentionally preserved from v7.1.4 because it tested clean on hardware.
- MP3/FLAC are not enabled yet; this version prepares the API and UI contract.
- PlatformIO was not run in this environment.
