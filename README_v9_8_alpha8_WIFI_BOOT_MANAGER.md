# IQ200 OS v9.8-alpha8 — WiFi Boot Manager

## Added
- Loads saved SSID/password and WiFi policy from NVS during boot.
- AutoConnect enabled by default.
- After 15 seconds without STA connection, starts fallback AP `IQ200-OS`.
- Reconnect continues every 10 seconds while AP remains available.
- AP+STA Web UI remains reachable at `192.168.4.1`.
- Persistent AutoConnect and Fallback AP settings.
- Forget saved profile from CLI or Web UI.

## Commands
```text
wifi status
wifi stats
wifi auto on
wifi auto off
wifi fallback on
wifi fallback off
wifi forget
wifi boot
wifi save <ssid> <password>
wifi load
```

## Boot behavior
1. Saved profile + AutoConnect ON: start STA.
2. Connected within 15 seconds: Web UI available on STA IP and `iq200.local`.
3. Not connected within 15 seconds + Fallback ON: start `IQ200-OS` AP and continue STA reconnect attempts.
4. No saved profile + Fallback ON: start AP immediately.

## Web API
- `GET /api/wifi/status`
- `POST /api/wifi/auto` (`enabled=1|0`)
- `POST /api/wifi/fallback` (`enabled=1|0`)
- `POST /api/wifi/forget`
