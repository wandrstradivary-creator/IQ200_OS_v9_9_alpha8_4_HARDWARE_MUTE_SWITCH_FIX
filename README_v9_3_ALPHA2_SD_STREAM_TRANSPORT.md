# IQ200 OS v9.3-alpha2 — SD STREAM TRANSPORT

## Added

- `SDManager::Stream` for long-lived decoder file sessions.
- FLAC `dr_flac` callbacks now use `SDManager::Stream::read/seek`.
- SD read/seek/close/retry/byte counters in `[SDM]` diagnostics.
- Up to three short retries for transient zero-byte reads before the decoder reports SDERR.

## Changed

- FLAC no longer uses `drflac_open_file()` / VFS `fopen(/sd/...)`.
- The decoder opens the media path through Arduino SD under SDManager arbitration.
- Stability probe uses `SDManager::open()`.

## Disabled

- Periodic `/iq200/stability.probe` writes while RT audio is active.
- Direct FLAC access through stdio/VFS.
- Untracked FLAC file reads that previously left `[SDM] open=0 read=0`.

## Preserved

- Artwork is prepared once before decoder start and cached per album.
- Auto Next EOF Guard.
- Segmented VU, Peak Hold and independent UI scheduler.

## Expected diagnostics

```text
[AUDIO-RT] v9.3-alpha2 SD stream transport task core=0 framesPerRead=4096
[SDM] ... open=1 ... read=... bytes=... retry=0 rfail=0 seek=... close=...
```

During playback the stability probe must not increment SD opens or create `stability.probe` traffic.

## Known limitations

- Full card remount/resume after a hard SPI/card reset is not implemented in this stage.
- Other legacy services still contain direct `SD.open()` calls; playback guards prevent them from running concurrently, and they will be migrated incrementally.
