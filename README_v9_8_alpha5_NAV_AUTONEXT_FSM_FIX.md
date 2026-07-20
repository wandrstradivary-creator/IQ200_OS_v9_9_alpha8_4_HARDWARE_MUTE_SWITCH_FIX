# IQ200 OS v9.8-alpha5 — Navigation / AutoNext FSM Fix

## Fixed

- Manual `next` / `prev` now cancels a queued EOF AutoNext request.
- Navigation preview owns the decoder STOP transition explicitly.
- A controlled stop at 100% is no longer interpreted as natural EOF.
- AutoNext is blocked while Navigation preview/commit is active.
- Duplicate playback starts and `IQPlayerCore already active/busy` caused by the NAV/EOF race are prevented.

## Test

1. Start WAV, FLAC, and MP3 playback.
2. Press Web UI `Prev` rapidly 10–20 times.
3. Wait for the 450 ms navigation commit.
4. Confirm exactly one decoder start after commit.
5. Confirm the log does not contain an EOF AutoNext between `Navigation preview` and `Navigation commit`.
6. Let a track finish naturally and confirm AutoNext still works.

Expected controlled stop:

```text
[NAV2] preview begin: stopping decoder once
[CORE0] ... RT stopped by control/handoff ...
[NAV2] commit selected=...
[HANDOFF] start OK ...
```

Expected natural EOF:

```text
[CORE0] ... RT finished 100%
[AUTONEXT] EOF queued ...
[HANDOFF] NEXT selected ... source=EOF
```
