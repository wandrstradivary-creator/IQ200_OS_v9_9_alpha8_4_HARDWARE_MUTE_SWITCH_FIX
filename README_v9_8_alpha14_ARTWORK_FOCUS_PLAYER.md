# IQ200 OS v9.8-alpha14 — Artwork Focus Player

Player screen rebuilt to the approved 480x320 mockup.

## Fixed geometry
- Header: 0,0,480,42
- Artwork: 16,54,190,180
- Title: 222,62,242,34
- Artist: 222,100,242,24
- Album: 222,130,242,24
- Codec/rate/bits/channels: 222,158
- VU L: 222,170,242,12
- VU R: 222,188,242,12
- Current/total time: 222,212
- Repeat mode: 356,212
- Volume: 420,212
- Elapsed time: 16,246
- Progress: 72,244,336,16
- Total time: 414,246
- Transport controls: y=297, centers 48/144/240/336/432

## Changes
- Removed progress percentage from LCD and Web Player.
- Elapsed and total time are shown around the progress bar.
- Added artist and album lines derived from the media path.
- Added codec, sample rate, bit depth and channel line.
- Added two independent segmented VU rows.
- Added five compact transport glyphs.
- Artwork Focus is now the canonical layout; old stored layouts are ignored.
- SD Concurrency Guard from alpha13 remains unchanged.
