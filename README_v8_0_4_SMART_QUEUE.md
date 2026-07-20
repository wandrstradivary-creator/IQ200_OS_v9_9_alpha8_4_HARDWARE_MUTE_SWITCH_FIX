# IQ200 OS v8.0.4 SMART QUEUE X10THINK

Base: v8.0.3 UPDATE ENGINE.

## Added

- Smart Queue modes on top of QueueManager.
- `qshuffle`, `shuffle`, `smartshuffle` toggle smart shuffle.
- `repeat off`, `repeat one`, `repeat all` set repeat mode.
- `repeat`, `qmode`, `mode` show current queue mode.
- Queue save/load now persists:
  - `SHUFFLE=0/1`
  - `REPEAT=0/1/2`
- `qnext` respects:
  - repeat one
  - repeat all
  - repeat off
  - smart shuffle with small recent-history avoidance.

## Notes

- Default repeat mode is `ALL` to preserve the old wrap-around behavior.
- RT Audio path was not changed.
- SD database engine was not changed.
