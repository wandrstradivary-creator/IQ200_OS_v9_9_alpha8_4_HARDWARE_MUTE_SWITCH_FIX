# IQ200 OS v9.8-alpha18 — Web Artwork

- Web Now Playing shows the same current JPEG/PNG artwork selected by ArtworkCache.
- `/api/status` and `/api/player` expose `artworkReady`, `artworkGeneration`, and `artworkUrl`.
- `GET /api/artwork` serves the immutable current image directly from the PSRAM LRU cache.
- No second SD open/read is performed for Web artwork.
- Browser uses `object-fit: contain`, so the entire cover remains visible without cropping.
- CD fallback remains visible when no cover is ready.
- Alpha17 fit-center, alpha16 audio-safe marquee, and alpha13 SD concurrency protections are preserved.
