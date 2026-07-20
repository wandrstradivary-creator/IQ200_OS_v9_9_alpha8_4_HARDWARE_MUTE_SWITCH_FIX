# IQ200 OS v9.9-alpha6 DEDICATED WEB + ARTWORK GUARD

## Confirmed alpha5 failure

The alpha5 decoder fairness fix covered steady-state playback, but Web still
went `Offline` immediately when a local FLAC was started.

The actual startup path remained synchronous inside the same Core 0 worker as
the WebServer:

1. stop the previous decoder and wait up to 1500 ms;
2. load up to 6 MB of Artwork from SD;
3. wait up to 900 ms for the TFT artwork render;
4. save resume state and open/prefetch the FLAC;
5. only then return to `server.handleClient()`.

The first `/api/status` request therefore exceeded the browser's 1800 ms
deadline. A large `/api/artwork` response could then block the synchronous HTTP
server again.

## Alpha6 changes

- WebServer now has a dedicated 8192-byte Core 0 task at priority 2.
- The blocking service worker remains priority 1; RT local audio remains
  priority 5; WebRadio remains priority 2.
- The Core 0 worker retains a Web fallback if task creation fails.
- `/api/status` uses a zero-wait Artwork mutex guard.
- `/api/artwork` waits at most 5 ms for the mutex, serves at most 256 KiB, and
  has a 900 ms total send budget.
- Larger covers remain available to the device TFT but Web uses the CD fallback
  until a future thumbnail pipeline is added.
- Browser Artwork errors are retryable and do not poison the generation cache.
- Web task running state, loop count and stack high-water are exposed through
  `/api/status` and `web info`.
- The alpha5 one-tick FLAC/MP3/WAV fairness windows remain enabled.

## Build

```powershell
pio run -e iq200-radio
```

If LovyanGFX is incomplete:

```powershell
Remove-Item -Recurse -Force .pio\libdeps\iq200-radio\LovyanGFX
pio run -e iq200-radio
```

## Device acceptance test

1. Open Web UI and start a local FLAC from Library.
2. Confirm the page never shows `Offline`, including during the first two
   seconds while Artwork and the decoder start.
3. Open `/api/status` during playback and confirm:
   - `webTaskRunning: true`;
   - `webTaskLoops` continuously increases;
   - `webTaskStackHighWater` remains safely above zero;
   - `webTickGapMaxMs` normally stays below 500 ms.
4. Repeat with a track whose JPEG/PNG is larger than 256 KiB. Web should show
   the CD fallback, while playback and REST remain responsive.
5. Run Next/Previous, volume, EQ, Library paging and WebRadio/local handoff.
6. Confirm audio `underruns` and `shortWrites` do not increase.
