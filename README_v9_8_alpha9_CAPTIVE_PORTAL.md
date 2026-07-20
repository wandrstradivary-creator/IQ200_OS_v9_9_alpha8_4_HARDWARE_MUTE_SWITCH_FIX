# IQ200 OS v9.8-alpha9 — Captive Portal

- DNS wildcard redirect on AP/AP+STA
- Android/iOS/Windows captive portal probe endpoints
- Unknown HTTP paths redirect to http://192.168.4.1/ while AP is active
- Existing WiFi Boot Manager, AutoConnect, Fallback AP and AP+STA retained

## Test
1. Forget the IQ200-OS network on the phone/PC.
2. Boot without a reachable saved network.
3. Join IQ200-OS.
4. The WiFi setup page should open automatically.
5. Manual fallback remains http://192.168.4.1/
