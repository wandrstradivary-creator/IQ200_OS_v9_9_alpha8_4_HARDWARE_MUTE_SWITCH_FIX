# IQ200 OS v9.9-alpha4 — WebRadio Async Stability

Based on v9.9-alpha3 WebRadio Crash Fix and the alpha47 Resume List Position
Fix. The stable local-player source remains available separately.

## Web UI freeze fix

- `POST /api/radio/play` no longer performs local-player shutdown, DNS lookup,
  HTTP connection, playlist resolution, or I2S setup in the REST handler.
- Play and Stop use a one-slot FreeRTOS command mailbox and return `202`
  immediately. The most recent user command wins.
- Only `webradio_rt` calls ESP32-audioI2S methods, including volume changes.
- The radio task priority is reduced and it yields a bounded Core0 window for
  WebServer and normal services while streaming.
- Browser status polling has request timeouts and in-flight guards, preventing
  a slow request from creating an unlimited chain of overlapping fetches.

## Audio ownership

- WebRadio takeover marks the local decoder stop as a control transition, so
  it cannot trigger local Auto Next accidentally.
- A new local Play request queues WebRadio Stop and waits non-blockingly for
  I2S release before opening FLAC/MP3/WAV.
- `stopSong()` remains guarded by a successfully created radio session.

## RAM and concurrency

- Cross-task WebRadio status now uses fixed-size character buffers and a locked
  snapshot instead of exposing mutable `String` references to WebServer.
- REST diagnostics expose radio command, reconnect, and task-stack counters.
- Stream metadata is bounded to prevent an ICY title from growing RAM without
  limit.

## Included base fix

- Resume restores both the saved track path and the real PlaylistManager index.
- Next and Auto Next continue from the restored list position.

## PlatformIO

Environment: `iq200-radio`

```powershell
pio run -e iq200-radio
```

If LovyanGFX was previously installed incompletely, remove `.pio`, run
`pio system prune --cache -f`, then build again from a short project path.

## Hardware test

1. Start a local FLAC, then press WebRadio Play. The REST response must return
   immediately and status must advance `QUEUED -> CONNECTING -> BUFFERING -> PLAYING`.
2. While the station plays, keep the Web UI open for 10 minutes. Tabs, station
   status, `/api/status`, and `/api/radio/status` must remain responsive.
3. Switch rapidly between three stations. The last selected station must win;
   no panic, duplicate I2S owner, or frozen UI is allowed.
4. Press Stop, then play a local FLAC/MP3/WAV without rebooting.
5. While WebRadio plays, select a local Library track. WebRadio must release I2S
   and the selected local track must start without local Auto Next firing.
6. Repeat Play/Stop 30 times and compare Heap plus `radioStackHighWater` before
   and after. There must be no monotonic leak or command drops.
