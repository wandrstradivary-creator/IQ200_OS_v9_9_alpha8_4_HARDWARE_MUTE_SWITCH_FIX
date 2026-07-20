# IQ200 OS v7.2.1 ALIAS + PLAY GUARD X10THINK

Changes:
- Added short Serial aliases:
  - `st` -> `status`
  - `hl` -> `health`
  - `rb` -> `reboot`
  - `now` -> `player`
  - `mi` / `meta` -> `minfo`
  - `mplay` -> `play`
  - `mstop` -> `stop`
- Added UI-side play guard: repeated `play/mplay/wav` while audio is active is ignored.
- Added Core0-side play guard: repeated WAV start request cannot spawn a second RT task.
- After WAV finish, UI progress resets to stopped/0 instead of showing a stale 100%.
- Version banner updated to v7.2.1.

Base: v7.2 MEDIA FOUNDATION.
