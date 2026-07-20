# IQ200 OS v9.5-alpha2 — Progress Widget 2.0

This release optimizes only the Player progress widget.

## Change

The previous implementation cleared and redrew the complete progress rectangle every second. The new implementation keeps the frame on-screen and updates only the pixel strip that changed.

- normal playback: paint only the new completed strip;
- seek/restart: clear only the obsolete strip;
- first Player draw/theme refresh: initialize the complete progress widget once;
- percentage text: refresh once per second.

## Verification

Run:

```text
ui reset
play
```

After 30–60 seconds:

```text
ui
```

Compare the `progress avg` value with the previous ~6 ms baseline.

## Frozen subsystems

No changes were made to FLAC RT, SD 16 MHz, 128 KB prefetch, SD Recovery, Auto Next, Theme Engine, VU, Black Box or Event Queue.
