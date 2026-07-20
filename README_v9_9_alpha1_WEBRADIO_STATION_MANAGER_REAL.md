# IQ200 OS v9.9-alpha1 WebRadio Station Manager REAL

Implemented in firmware:
- Web Radio tab with list/editor/delete/favorite.
- REST endpoints under /api/radio/.
- Persistent ESP32 NVS storage, up to 96 stations.
- Import: M3U, URL-per-line, TSV, semicolon CSV, comma CSV.
- Export: JSON and M3U.
- Included example: data/iq200/radio/playlist_example.tsv.

This release manages stations. HTTP audio playback is the next subsystem.
