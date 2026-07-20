# IQ200 OS v9.8-alpha1 — WiFi Foundation

Adds a persistent Wi-Fi manager without changing Audio Core.

Commands:
- `wifi scan`
- `wifi status` / `wifi ip`
- `wifi connect <ssid> <password>`
- `wifi apsta <ssid> <password>`
- `wifi load` / `wifi sta`
- `wifi disconnect`
- `wifi ap`
- `wifi off`
- `web on`

The STA profile is stored in NVS. When connected to a router, Web UI is available by IP and via `http://iq200.local/` when mDNS starts successfully. AP mode remains available at `http://192.168.4.1/`.
