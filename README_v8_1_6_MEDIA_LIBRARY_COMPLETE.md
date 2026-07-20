# IQ200 OS v8.1.6 MEDIA LIBRARY COMPLETE X10THINK

Added SD-backed library index foundation on top of v8.1.5.

## Added
- `LibraryManager.h`
- `artists / artist`
- `albums / album` integrated with library index requests
- `genres / genre`
- `folders / folder`
- `recent`
- `most / mostplayed`
- `stats / libstats`
- `libbuild` to build `artist.idx`, `album.idx`, `genre.idx`, `folder.idx`, `library.meta` from the existing SD media database
- automatic library index build after full SD database scan

## SD files
- `/iq200/db/artist.idx`
- `/iq200/db/album.idx`
- `/iq200/db/genre.idx`
- `/iq200/db/folder.idx`
- `/iq200/db/recent.db`
- `/iq200/db/mostplayed.db`
- `/iq200/db/library.meta`

## Notes
- No RT Audio changes.
- No extra SD media scan: library indexes are built from `media.idx + media_###.db`.
- Artist/album/genre parsing is path-based foundation; real tag parser can replace it later.
