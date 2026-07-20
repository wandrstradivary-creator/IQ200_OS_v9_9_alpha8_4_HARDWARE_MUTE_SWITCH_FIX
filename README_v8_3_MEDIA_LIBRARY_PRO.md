# IQ200 OS v8.3 — Media Library Pro

## Goal
Artists / Albums / Genres / Folders / Recent / Most Played / Search must work through SD-backed indexes, not by rescanning media DB volumes during UI navigation.

## Added
- `LibraryManager` upgraded to v8.3 Media Library Pro.
- Builds compact indexes:
  - `/iq200/db/library/artist.idx`
  - `/iq200/db/library/album.idx`
  - `/iq200/db/library/genre.idx`
  - `/iq200/db/library/folder.idx`
  - `/iq200/db/library/search.idx`
- Builds track maps:
  - `/iq200/db/library/artist_tracks.idx`
  - `/iq200/db/library/album_tracks.idx`
  - `/iq200/db/library/genre_tracks.idx`
  - `/iq200/db/library/folder_tracks.idx`
- `find <text>` now uses `/iq200/db/library/search.idx`.
- `libbuild` rebuilds all library indexes from `/iq200/db/media/media.idx`.
- Existing ScanService / DatabaseService / SmartResume / Diagnostics Pro are preserved.

## Expected flow
1. Boot.
2. `db` confirms media DB.
3. `libbuild` rebuilds library indexes.
4. `artists`, `albums`, `genres`, `folders`, `recent`, `most`, `find <text>` read index files.

## Notes
- Search is index-backed and should be faster than scanning all media volumes.
- Recent/Most remain SD DB files prepared for runtime history updates.
