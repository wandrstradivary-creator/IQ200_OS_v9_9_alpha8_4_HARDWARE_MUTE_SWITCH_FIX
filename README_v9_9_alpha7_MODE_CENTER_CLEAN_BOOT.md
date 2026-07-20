# IQ200 OS v9.9-alpha7 — Mode Center Clean Boot

This build separates Local Player and WebRadio into mutually exclusive runtime platforms. Changing a mode writes the target to NVS, shuts down the active platform and reboots the ESP32-S3. Only the selected platform is initialized after restart.

## Mode matrix

| Mode | Initialized | Kept off |
|---|---|---|
| Mode Center | TFT, encoders, NVS selector | WiFi, Web, SD, DB, decoders, I2S |
| Local Player | SD, DB, local FLAC/MP3/WAV, Resume, Artwork, Web | WebRadio decoder/task |
| WebRadio | WiFi, Web, station store, ESP32-audioI2S, lightweight TFT UI | SD, DB, local decoders, Resume, EQ, Artwork |
| Bluetooth | Future placeholder | Not bootable |
| FM / Radio | Future placeholder | Not bootable |

ESP32-audioI2S is now allocated lazily inside `RadioService::begin()`, so Local Player and Mode Center do not reserve its decoder/stream RAM.

## Controls and recovery

- In Mode Center, rotate NAV and press NAV or VOL to select a platform.
- In Local Player or WebRadio, hold both encoder buttons for two seconds to return to Mode Center through a clean reboot.
- Hold NAV for at least 0.7 seconds during reset to force Mode Center.
- A selected platform is marked healthy after 10 seconds. Three consecutive early boot failures automatically restore Mode Center.

## Web and REST

- `GET /api/mode` reports the active mode, boot health and available/future modes.
- `POST /api/mode/switch` with `mode=center|local|webradio` schedules a clean reboot.
- Local library, artwork and player REST endpoints return HTTP 409 outside Local Player mode.
- WebRadio play/stop endpoints return HTTP 409 outside WebRadio mode.
- The Web UI hides platform-incompatible tabs and loads only the selected platform's data.

## Validation

- 26 source-level regression tests cover Web fairness, Artwork locking, asynchronous WebRadio, Resume list position and clean-boot isolation.
- Embedded Web UI JavaScript passes a Node syntax check.
- A firmware compile was not run in the delivery environment because PlatformIO is unavailable there.

## Local build note

Use the short environment name:

```text
pio run -e iq200-radio
```

If LovyanGFX reports missing internal headers such as `DataWrapper.hpp` or `pgmspace.h`, its local PlatformIO dependency cache is incomplete. Remove the project `.pio` build/dependency cache and rebuild so PlatformIO downloads a clean LovyanGFX 1.1.16 package. The application source does not patch library internals.
