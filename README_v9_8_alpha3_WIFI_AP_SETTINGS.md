# IQ200 OS v9.8-alpha3 WiFi AP Settings

Maintenance release based on v9.8-alpha2.

## Changes
- PlatformIO environment renamed to the current v9.8-alpha3 target.
- `IQ200_VERSION` build define synchronized.
- Serial boot banner synchronized.
- Display/About version synchronized.
- Web API version synchronized.
- WiFi AP settings and AP+STA behavior retained from alpha2.

## First test
1. Build and upload from PlatformIO.
2. Open Serial Monitor at 115200.
3. Run `net ap`.
4. Join `IQ200-OS`.
5. Open `http://192.168.4.1/`.
6. Open the WiFi tab, scan, and connect in AP+STA mode.
