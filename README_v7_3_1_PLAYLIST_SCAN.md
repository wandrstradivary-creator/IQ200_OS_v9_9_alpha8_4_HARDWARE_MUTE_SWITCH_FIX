# IQ200 OS v7.3.1 PLAYLIST SCAN X10THINK

Changes:
- Added `plscan` command.
- `plscan` scans SD root and builds playlist from `.wav`, `.mp3`, `.flac`.
- Keeps stable v7.3 RT WAV playback path unchanged.
- Adds path normalization for root files.
- Updates help/footer/version strings to v7.3.1.

Test:
1. `sd`
2. `plscan`
3. `pl` or `mi`
4. `play`
5. `next` / `prev`
