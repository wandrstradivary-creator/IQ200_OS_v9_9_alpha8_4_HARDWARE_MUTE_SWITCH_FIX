# IQ200 OS v8.6 — Web UI / OTA Base — X10THINK

Hardware target: ESP32-S3 N16R8 + ILI9488 + PCM5102 + SD.

## Added in v8.6

- `ConnectivityManager` foundation.
- `WebServerService` with real Arduino `WebServer` code.
- Minimal browser UI at `/`.
- REST endpoints:
  - `GET /api/status`
  - `GET /api/player`
  - `GET /api/diagnostics`
  - `POST /api/play`
  - `POST /api/stop`
  - `POST /api/next`
  - `POST /api/prev`
  - `POST /api/scan`
  - `POST /api/volume?v=42`
- OTA SD validation hooks:
  - `ota info`
  - `ota sd`
- Radio framework commands:
  - `radio`
  - `radio stop`
- Network commands:
  - `net ap`
  - `net off`
  - `web`
  - `web on`
  - `web off`

## Notes

This is the Web UI / OTA base layer. The Web server is intentionally low-priority and driven from Core0 service ticks. It does not block the audio path or ScanService. SD OTA full flashing is intentionally guarded: v8.6 validates command flow and SD layout first; full `Update.writeStream()` can be enabled after v8.7 burn-in.

Expected test flow:

```text
net ap
web on
web
```

Then open:

```text
http://192.168.4.1/
```

## Preserved from previous versions

- v8.2.2 ScanService
- v8.2.3 Partial Renderer
- v8.2.4 DatabaseService
- v8.2.5 SmartResume
- v8.2.6 DiagnosticsPro
- v8.3 MediaLibraryPro
