# IQ200 OS v7.3.5 SD MEDIA ART DB X10THINK

- Media database is always stored on SD: `/iq200/db/media_###.db`.
- Album art database is stored on SD: `/iq200/db/art_###.db`.
- Multivolume DB: rotates every 512 lines, so database size is SD-backed, not RAM-limited.
- Priority scan/write order: MP3 -> FLAC -> WAV.
- Album art priority: cover/folder/album BMP/JPG in the track folder, then same-name BMP/JPG, then `CD_DISK_INTERNAL` fallback.
- Boot loads existing DB if present, without full SD scan.
- `scan` / `rescan` rebuilds the DB.
- `db` shows DB status.
- `dbload` reloads playlist from DB.
- `dbclear` removes DB volumes.

Note: active RAM playlist is a window for UI/navigation; full media library is stored in multivolume SD DB.
