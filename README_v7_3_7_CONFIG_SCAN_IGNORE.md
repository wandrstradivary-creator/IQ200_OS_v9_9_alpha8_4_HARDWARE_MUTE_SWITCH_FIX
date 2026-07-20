# IQ200 OS v7.3.7 CONFIG SCAN IGNORE X10THINK

Added configurable SD scan ignore list.

## New SD config

`/iq200/config/scan_ignore.txt` is created automatically if missing.

Default ignored folders:

- System Volume Information
- $RECYCLE.BIN
- LOST.DIR
- .Trash
- .Trashes
- .fseventsd
- .Spotlight-V100
- Android/data
- Android/obb
- DCIM/.thumbnails
- cache
- tmp
- Temp

## Behavior

- Recursive media scan skips ignored branches.
- Rescan reloads the ignore config from SD.
- This speeds up large cards and reduces WDT risk.
- RT Audio was not changed.
