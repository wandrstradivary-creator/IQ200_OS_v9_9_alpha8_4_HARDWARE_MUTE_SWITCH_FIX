# IQ200 OS v7.7.0 DATABASE ENGINE PRO X10THINK

Base: v7.6.0 DATABASE ENGINE.

Changes:
- Boot DBTEST disabled by default (`IQ200_DB_TEST 0`).
- `dbtest` / `testdb` / `dbcheck` remain available manually.
- Added `find <text>` command to search the SD-backed media database.
- Find uses `/iq200/db/media.idx` + `media_###.db`; it does not rescan SD.
- Boot remains fast when `media.meta` + `media.idx` are valid.
- Manifest build string updated to v7.7.0.
- RT Audio path unchanged.

Test commands:

```text
db
find кукушка
find queen
dbtest
play
```
