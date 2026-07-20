# IQ200 OS v9.8-alpha17 — Artwork Fit Center

- Album artwork is scaled proportionally to fit completely inside the 190x180 player area.
- Image is centered horizontally and vertically.
- No top-left crop.
- Aspect ratio is preserved; unused space is filled with the active theme background.
- JPEG and PNG dimensions are read directly from their headers.
- Audio-safe marquee and SD concurrency guards from alpha16 remain unchanged.

Expected diagnostic log:
```
[ART][FIT] src=800x800 box=190x180 scale=0.225 pos=21,54 ok=1
```
