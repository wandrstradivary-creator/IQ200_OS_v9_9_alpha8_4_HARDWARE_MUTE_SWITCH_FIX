# IQ200 OS v7.3.8 DB TEST MODE X10THINK

Temporary database validation build.

## Added
- `#define IQ200_DB_TEST 1` in `src/main.cpp`.
- Boot-time DB self-test when SD database exists.
- Commands: `dbtest`, `testdb`, `dbcheck`.
- `db` output includes `testErrors`.
- Validation checks multivolume `media_###.db` and `art_###.db` line format, codecs, and paths.
- WDT-safe: calls `vTaskDelay(1)` while reading DB files.

## Remove before release
Set:

```cpp
#define IQ200_DB_TEST 0
```

Then boot does not run verbose DBTEST. The manual `dbtest` command can also be removed later.
