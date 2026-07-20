# IQ200 OS v9.8-alpha37 ART RELIABILITY AUDIT

- Unified resolver for album and per-track artwork.
- Multiple images in one folder no longer suppress artwork.
- Case-insensitive JPG/JPEG/PNG extension scan.
- Filters thumbnails/icons and penalizes rear/back images.
- Artwork limit raised to 6 MB in PSRAM.
- JPEG decoder fallback across 1, 1/2, 1/4 and 1/8 scales.
- TFT decode retries after transient allocation/decode failure.
- No nested TFT transaction during image decode.
- Web and TFT continue to use one immutable ArtworkCache entry.
