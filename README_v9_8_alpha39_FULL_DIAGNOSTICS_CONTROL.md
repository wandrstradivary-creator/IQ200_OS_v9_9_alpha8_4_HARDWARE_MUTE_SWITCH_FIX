# IQ200 OS v9.8-alpha39 FULL DIAGNOSTICS CONTROL

Commands:
- `diagnostics off` — fully stops UART/USB serial output and ESP-IDF logs. Saved in NVS.
- `diagnostics on` — re-enables output (available from Web Console while UART is off).
- `diagnostics status` — shows current state.
- `log off/on` — keeps legacy behavior: only periodic process profiler.

When full diagnostics are off, playback/UI/Web continue to work. Serial commands are unavailable until diagnostics are enabled again from Web Console. Critical state is still retained in BlackBox memory for later inspection.
