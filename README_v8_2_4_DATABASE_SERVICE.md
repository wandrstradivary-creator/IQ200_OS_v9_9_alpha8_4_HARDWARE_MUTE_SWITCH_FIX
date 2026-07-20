# IQ200 OS v8.2.4 — Database Service X10THINK

## Goal
Split the SD database layer into separate services/families so media scanning,
library indexes, favorites, queue and resume do not fight over one flat `/iq200/db` namespace.

## SD layout

```text
/iq200/db/
  media/
    media.meta
    media.idx
    media_000.db ...
    art_000.db ...
  library/
    library.meta
    artist.idx
    album.idx
    genre.idx
    folder.idx
    recent.db
    mostplayed.db
  favorites/
    favorites.db
  queue/
    queue.db
  resume/
    resume.dat
  art/
    reserved for future art DB/cache records
```

## Implemented

- Added `DatabaseService.h` as the central DB coordinator.
- Media DB moved to `/iq200/db/media/`.
- Library indexes moved to `/iq200/db/library/`.
- Favorites DB moved to `/iq200/db/favorites/`.
- Queue DB moved to `/iq200/db/queue/`.
- Resume DB moved to `/iq200/db/resume/`.
- Boot now starts DB service only after SD mount.
- `db` command prints DatabaseService layout.
- Safe legacy migration from old flat files, for example:
  - `/iq200/db/media.idx` → `/iq200/db/media/media.idx`
  - `/iq200/db/favorites.db` → `/iq200/db/favorites/favorites.db`
  - `/iq200/db/queue.db` → `/iq200/db/queue/queue.db`
  - `/iq200/db/resume.dat` → `/iq200/db/resume/resume.dat`

## Notes

This is still the v8.x WAV/MP3/FLAC architecture branch. The goal of v8.2.4 is
service separation and SD layout stability, not decoder replacement.
