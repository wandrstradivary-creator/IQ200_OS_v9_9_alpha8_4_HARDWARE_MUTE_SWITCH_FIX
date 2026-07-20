# IQ200 OS v9.8-alpha29 RGB565 Artwork Fit

- JPEG is decoded once into a PSRAM RGB565 sprite.
- The decoded image is resized to fill the 190x180 artwork box while preserving aspect ratio.
- No crop and no tiny 1/8 thumbnail.
- TFT uses `pushSprite()` for the final RGB565 frame.
- Web artwork and alpha26 audio pipeline are unchanged.

Expected log:

```text
[ART][RGB565] src=800x800 decoded=100x100 out=180x180 pos=21,54 scale=0.225 OK
```
