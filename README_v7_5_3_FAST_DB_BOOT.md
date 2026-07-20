# IQ200 OS v7.5.3 FAST DB BOOT X10THINK

Changes:
- Fast validates `/iq200/db/media.meta` on boot.
- If manifest and media volumes are present, full SD scan is skipped.
- Loads playlist from multivolume DB directly.
- Keeps `scan/rescan` for forced rebuild.
- Adds DBFAST diagnostics in Serial.
- RT Audio unchanged.

Test:
1. Boot once after `scan` created DB.
2. Watch Serial for `[DBFAST] OK` and `full SD scan skipped`.
3. Run `db` to inspect manifest.
4. Run `rescan` only when SD content changed.
