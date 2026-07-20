# IQ200 OS v9.9-alpha8 — Commercial WebRadio UI

Implemented from the v9.9-alpha7.13 WebRadio Web Player baseline.

## Changes

- Redesigned 480×320 WebRadio player layout with station, artist, track, stream quality, LIVE state, artwork and dual VU.
- ICY metadata cleanup: removes StreamTitle wrappers, underscores, duplicate spaces and trailing service delimiters.
- Artist/title split using the standard `Artist - Track` separator.
- Independent low-cost marquee lines for station, artist and track.
- Stream codec detection for MP3, AAC, AAC+, OGG and FLAC.
- Bitrate and sample-rate fields added to the lock-free RadioService snapshot.
- WebRadio VU automatic gain control with fast attack and slow release; user gain/noise-gate settings remain active.
- Existing station artwork cache and safe fallback retained.
- No full-screen redraw in the VU or marquee paths.

## Verification

- Python regression suite: 38 passed.
- PlatformIO firmware compilation was not available in the build container; compile on the normal PlatformIO workstation before flashing.
