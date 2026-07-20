# IQ200 OS v9.8-alpha23 POST-SCAN PLAY FIX

Fixes playback not starting after a successful full SD scan.

After rebuilding and loading the playlist, ScanService now explicitly:
- restores MEDIA_STATE_READY when tracks exist;
- clears stale play/stop/handoff/navigation requests;
- clears audioBusy/audioPlaying/RT task mirrors;
- resets progress and VU state;
- publishes SCAN_READY handoff state.

Expected log after scan:

[SCANSVC] player finalized: state=READY busy=0 playReq=0 track=/Music/...
[SCANSVC] complete: ...

Then `play` or Web Library Play should start normally.
