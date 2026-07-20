# IQ200 OS v9.9-alpha2 — WebRadio Playback + Editor Fix

## Added
- HTTP/HTTPS WebRadio playback through ESP32-audioI2S 2.0.7.
- MP3/AAC and ICY stream support.
- Play/Stop buttons and live status in Web UI.
- Local MP3/FLAC/WAV playback is stopped before WebRadio takes I2S ownership.
- Automatic reconnect after stream interruption.
- Station name and ICY stream title callbacks.
- Last successfully selected station saved in NVS.

## Editor audit
The alpha1 editor looked correct but its button event handlers were not attached, and the import button ID did not match the JavaScript ID. Alpha2 fixes all bindings:
- Save / New / Delete
- Search / Refresh
- Import append / Replace all
- Export JSON / Export M3U
- Play / Stop / Status

Station validation now reports explicit errors: missing_name, invalid_url, duplicate_url, field_too_long, store_full, invalid_id.

## Test
1. Connect WiFi and open `http://iq200.local/`.
2. Open Web Radio.
3. Import `playlist.csv` text or create a station.
4. Press Save and verify it remains after reboot.
5. Press Play. Expected states: CONNECTING -> BUFFERING -> PLAYING.
6. Press Stop before returning to local Player.

Some `.m3u` links redirect to a real stream URL; the playback library handles common M3U/PLS and ICY streams. A dead or geo-blocked station will show ERROR/RECONNECTING.
