# IQ200 OS v9.5-alpha3 — Navigation Engine 2.0

## Added
- Coalesced manual `next` / `prev` navigation.
- In-memory preview selection without artwork load or decoder start.
- One decoder stop at the beginning of rapid navigation.
- Idle commit after 450 ms (configurable 200..1500 ms).
- Only the final selected track is opened and played.

## Commands
- `nav status`
- `nav preview on`
- `nav preview off`
- `nav delay 450`

## Runtime log
- `[NAV2] preview begin: stopping decoder once`
- `[NAV2] preview delta=... selected=...`
- `[NAV2] commit selected=... idle=... autoplay=...`

Audio Core, FLAC decoder, SD prefetch/recovery and Auto Next are unchanged.
