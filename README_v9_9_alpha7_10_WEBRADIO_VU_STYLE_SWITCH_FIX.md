# IQ200 OS v9.9-alpha7.10 — WebRadio VU Style Switch Fix

This build fixes Web UI switching between Line, Thin, Blocks, Dots, Neon and Center.
The browser keeps the selected value while the command is queued and accepts the
value from `/api/status` only after firmware confirmation or a bounded timeout.

Build with `pio run -e iq200-radio`.
