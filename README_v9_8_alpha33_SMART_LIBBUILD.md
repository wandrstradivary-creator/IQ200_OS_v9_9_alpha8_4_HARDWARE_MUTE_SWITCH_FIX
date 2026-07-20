# IQ200 OS v9.8-alpha33 SMART LIBBUILD

- `libbuild` checks media metadata and skips an unnecessary rebuild when all v3 indexes are current.
- `libbuild force` always rebuilds Artists, Albums, Genres, Folders, track maps and Search index.
- Does not perform an SD media scan.
- Reports UP_TO_DATE or REBUILT with counts and elapsed time.
- Existing Recent and Most Played databases are preserved.

Expected logs:

```text
[LIBBUILD] up-to-date tracks=280 sourceScan=... check=... ms; rebuild skipped
[LIB] build ok=1 mode=UP_TO_DATE ...
```

or:

```text
[LIBBUILD] rebuild start force=0 ...
[LIBBUILD] rebuild OK tracks=280 artists=... search=OK ...
[LIB] build ok=1 mode=REBUILT ...
```
