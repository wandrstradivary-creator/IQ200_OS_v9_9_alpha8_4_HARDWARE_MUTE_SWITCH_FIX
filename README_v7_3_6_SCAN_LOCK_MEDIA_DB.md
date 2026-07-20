# IQ200 OS v7.3.6 SCAN LOCK MEDIA DB

- SD scan shows a dedicated SCAN screen.
- During scan all media/process commands wait/are ignored until completion.
- Audio is stopped before full SD scan to avoid SPI/SD contention.
- `scan/rescan/dbscan/artscan` rebuilds `/iq200/db/media_###.db` and `/iq200/db/art_###.db`.
- Progress fields: message, files visited, tracks, elapsed time, progress bar.
- RT Audio path unchanged.
