# IQ200 OS v9.9-alpha5 WEB FLAC ONLINE FIX

## Fixed defect

Starting a local FLAC track could leave audio playing normally while the Web UI
reported `Offline` and REST requests stopped completing.

The local decoder task is pinned to Core 0 at priority 5. The WebServer runs in
the shared Core 0 worker at priority 1. The old decoder loop called
`taskYIELD()` only every eighth PCM block, but FreeRTOS `taskYIELD()` does not
schedule a ready task at a lower priority. A continuously ready decoder could
therefore starve the WebServer for the duration of the track.

## Changes

- FLAC now blocks for one RTOS tick after every successfully queued 4096-frame
  PCM block. At 44.1 kHz that block represents about 93 ms of queued audio, so
  the 1 ms service window stays inside the I2S buffer margin.
- MP3 and WAV use the same local-audio fairness contract.
- `webServerService.tick()` runs first whenever the Core 0 worker is scheduled.
- `/api/status` exposes `webTicks`, `webTickGapLastMs` and a rolling
  `webTickGapMaxMs` (10-second window).
- Web Diagnostics shows `Web gap max/10s` and warns above 500 ms.
- No additional task or stack was allocated; RAM use is unchanged apart from a
  few counters.

## Build

Use the short PlatformIO environment:

```powershell
pio run -e iq200-radio
```

If the previous LovyanGFX download is incomplete, delete only the damaged
dependency cache and build again:

```powershell
Remove-Item -Recurse -Force .pio\libdeps\iq200-radio\LovyanGFX
pio run -e iq200-radio
```

## Device acceptance test

1. Boot, enable Web and open `/api/status`.
2. Start a local FLAC from the Web UI. Keep it playing for at least 10 minutes.
3. During playback, switch through Now Playing, Library, EQ, Artwork and
   Diagnostics; issue Play/Pause, Next/Previous and volume commands.
4. Confirm the page never changes to `Offline` and REST requests complete.
5. In Diagnostics, check `Web gap max/10s`: target below 500 ms.
6. Confirm `underruns` and `shortWrites` do not increase.
7. Repeat with FLAC 44.1/16, 48/24, 88.2/24, 96/24 and with MP3/WAV.
8. Stop local playback, start WebRadio, then return to a local FLAC to verify
   I2S handoff remains stable.

## Static tests

```powershell
python -m unittest discover -s tests -v
```
