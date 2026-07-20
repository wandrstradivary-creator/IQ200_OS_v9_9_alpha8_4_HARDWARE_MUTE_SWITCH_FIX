# IQ200 OS v8.1.9 BUILD HYGIENE X10THINK

Fixes and stabilizes the v8.1.x build base before the next Media Center UI work.

- Version strings synchronized to v8.1.9.
- LovyanGFX remains pinned to 1.1.16.
- PlatformIO LDF/compat settings added.
- UI constructor checked and kept single/valid.
- RT Audio not changed.

Before rebuilding after LovyanGFX dependency changes, delete `.pio` or at least the cached LovyanGFX folder.
