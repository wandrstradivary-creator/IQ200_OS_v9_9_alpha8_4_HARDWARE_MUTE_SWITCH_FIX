# IQ200 OS v9.8-alpha28 TFT ARTWORK DECODE FIX

- Fixes artwork visible in Web but missing on TFT Player.
- Uses JPEG decoder-safe downscale steps: 1, 1/2, 1/4, 1/8.
- Adds one retry at 1/8 for large JPEG covers.
- Keeps fit-center positioning and Artwork Generation Lock.
- Audio pipeline from alpha26 is unchanged.

Expected log:
```
[ART][FIT] src=... box=190x180 requested=... decoder=0.125 ... ok=1
```
