# IQ200 OS v7.6 DATABASE ENGINE X10THINK

Added SD-backed database index engine.

## New/changed
- `/iq200/db/media.meta` summary remains the fast boot manifest.
- `/iq200/db/media.idx` maps database volumes to record counts.
- `loadPlaylist()` uses `media.idx` first, so boot no longer probes every possible `media_###.db`.
- `db` prints both `[DBMETA]` and `[DBIDX]`.
- `dbtest` checks that `media.idx` exists.
- Single-pass scan remains unchanged.
- RT Audio unchanged.

## Files
- `/iq200/db/media.meta`
- `/iq200/db/media.idx`
- `/iq200/db/media_###.db`
- `/iq200/db/art_###.db`
