# IQ200 OS v8.7.1 RECOVERY_FIX X10THINK

Runtime fix after real v8.7 burn-in log.

Fixed:
- Library rebuild after scan could fail with `vfs_fat: open: no free file descriptors`.
- LibraryManager no longer keeps five map files open while reading `media.idx` and `media_###.db`.
- Map/index files are appended one-at-a-time: max open file descriptors stays low during scan + libbuild.
- Version/banner updated to v8.7.1_RECOVERY_FIX.

Test commands:
```text
diag
stability
burnin start
scan
stability
burnin stop
libstats
find цой
```
