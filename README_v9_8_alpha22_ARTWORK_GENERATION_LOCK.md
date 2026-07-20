# IQ200 OS v9.8-alpha22 ARTWORK GENERATION LOCK

- Album artwork redraw is driven only by ArtworkCache generation changes.
- Volume, STOP, state, VU, progress and marquee partial refreshes never redraw artwork.
- Entering Player or changing track/theme still redraws artwork normally.
- Keeps alpha21 volume-only redraw and all prior SD/audio guards.
