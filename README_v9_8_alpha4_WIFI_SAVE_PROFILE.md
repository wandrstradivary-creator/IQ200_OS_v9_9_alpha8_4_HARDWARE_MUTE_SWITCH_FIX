# IQ200 OS v9.8-alpha4 — WiFi Save Profile

Added explicit WiFi profile persistence without connecting.

## Serial commands

```text
wifi save <ssid> <password>
wifi load
wifi status
```

`wifi connect` and `wifi apsta` still save automatically.

## AP Web UI

Open `http://192.168.4.1/`, select WiFi and use **Save profile** to store credentials without starting STA.

## REST API

```text
POST /api/wifi/save
ssid=<ssid>&password=<password>
```
