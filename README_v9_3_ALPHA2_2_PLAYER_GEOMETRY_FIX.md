# IQ200 OS v9.3-alpha2.2 — PLAYER GEOMETRY FIX

## Added
- 8 px safe gap between the progress bar and album artwork.

## Changed
- Player progress width: 360 px -> 292 px.
- Player soft keys Y position: 278 px -> 258 px.
- PLAY / STOP / BACK buttons now sit fully above the footer.

## Disabled
- Progress drawing inside the album-art region.
- Button overlap with the footer area.

## Preserved
- dr_flac API compatibility fix.
- SD stream transport and retry counters.
- Artwork cache and CD DISC fallback.
- Auto Next EOF guard.
- Segmented VU, Peak Hold and UI scheduler.
