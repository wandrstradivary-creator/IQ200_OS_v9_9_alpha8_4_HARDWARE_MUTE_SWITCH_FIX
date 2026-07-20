# IQ200 OS v9.8-alpha7 — Artwork LRU PSRAM

- 4-entry album artwork cache in PSRAM.
- LRU eviction.
- Searches cover/folder/front/album/AlbumArt/artwork names, then any JPG/JPEG/PNG in the album folder.
- Keeps decoded source bytes immutable for LovyanGFX JPEG/PNG rendering.
- Commands: `art`, `art info`, `art cache`, `art clear`, `art reload`.
- `art reload` executes SD lookup on Core0.
- Cache logs include slot, bytes, hit/load/evict counters and generation.
