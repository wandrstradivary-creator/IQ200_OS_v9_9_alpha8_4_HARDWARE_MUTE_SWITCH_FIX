# IQ200 OS v9.8-alpha46 — Web Player + Playlist 2.0

## Changes
- Added the active playlist directly below Now Playing.
- Current track is highlighted and kept visible.
- Clicking a row or Play starts that track without leaving Now Playing.
- Search, scroll position and page offset are stored in browser localStorage.
- Status polling updates only the active playlist selection and reloads the relevant 50-track window when necessary.
- Existing Library tab remains available.
- Audio, TFT, ART, EQ, Resume and SD logic are unchanged.

## Test
1. Open Web UI → Now Playing.
2. Scroll the playlist and start a track.
3. Verify the page stays on Now Playing and the active row is highlighted.
4. Press Next and confirm the list follows the new current track.
5. Reload the browser and verify the list position/search are restored.
