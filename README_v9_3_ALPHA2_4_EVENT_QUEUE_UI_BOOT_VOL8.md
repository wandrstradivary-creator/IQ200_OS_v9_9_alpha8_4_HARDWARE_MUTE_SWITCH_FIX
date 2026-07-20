# IQ200 OS v9.3-alpha2.4 EVENT QUEUE + UI SCHEDULER + BOOT VOL 8

## Added
- Coalescing for pending `EVT_WAV_PROGRESS` and `EVT_MONITOR` events.
- `evCoal` process-log counter.
- Deterministic Core1 scheduler rates.
- Safe boot volume forced to 8%.

## Changed
- Core1 handles at most six queued events per loop, preventing event floods from starving rendering/input.
- Serial command timeout reduced from the Arduino default to 10 ms.
- Input scheduler: 500 Hz; GUI: 200 Hz; animation: ~62 Hz; notifications: 20 Hz; service tick: 100 Hz.

## Disabled
- Duplicate queued progress/monitor events while one of the same type is already pending.
- One-second Core1 stalls caused by blocking serial line reads.
- Restored/saved high volume at power-on; startup is always 8%.

## Preserved
- SD exclusive stream transport.
- FLAC decoder buffers and callbacks.
- Auto Next EOF guard.
- Artwork cache and Player geometry.
