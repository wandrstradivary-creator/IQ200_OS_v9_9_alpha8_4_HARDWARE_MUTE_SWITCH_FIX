# IQ200 OS v9.9-alpha7.3 — Unsupported Stream Guard

Field testing confirmed stable Web control and switching across MP3 and AAC stations over HTTP and HTTPS. TLS temporarily reduced internal heap to roughly 69–77 KB and it recovered to about 116 KB after switching, with no visible leak.

The bundled ESP32-audioI2S decoder does not support the tested OGG/Opus and Ogg-FLAC streams. Previously these late protocol errors could enter the generic stall/EOF reconnect path. This build defers shutdown until the audio callback returns, stops the session safely and leaves it in:

```text
state=ERROR
error=unsupported_stream_format
```

Automatic reconnect is disabled for that session. A new supported station can be selected immediately. The unsupported counter is exposed in `/api/status` and `/api/radio/status`.
