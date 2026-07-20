# IQ200 OS v9.8-alpha41 ART PREPLAY SYNC

- TFT artwork is decoded and drawn in a bounded pre-play window before RT audio starts.
- Web and TFT continue to use the same compressed PSRAM cache.
- During playback, heavy JPEG/PNG decode remains blocked to protect audio.
- Added canonical parent-folder artwork fallback when the track folder has no image.
- Added `[ART][PREPLAY]` synchronization diagnostics.
