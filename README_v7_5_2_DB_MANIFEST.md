# IQ200 OS v7.5.2 DB MANIFEST X10THINK

Base: v7.5.1 Scan Pipeline Stable.

## Added
- `/iq200/db/media.meta` manifest after every `scan/rescan`.
- Fast DB summary at boot: tracks, volumes, art count.
- `db` command now prints `media.meta`.
- `dbclear` removes `media.meta` too.
- Single-pass scan remains unchanged: MP3 / FLAC / WAV / JPG / JPEG / BMP in one SD traversal.
- RT Audio unchanged.

## media.meta format
```text
# IQ200 MEDIA META v1
DB_VERSION=1
BUILD=v7.5.2
TRACKS=...
MP3=...
FLAC=...
WAV=...
ART=...
FILES=...
VOLUMES=...
LINES_PER_VOLUME=512
LAST_SCAN_MS=...
MODE=SINGLE_PASS
```

## Test
1. `scan`
2. `db`
3. `dbload`
4. `list`

Expected: `db` prints `[DBMETA]` lines and playlist loads without another full SD scan.
