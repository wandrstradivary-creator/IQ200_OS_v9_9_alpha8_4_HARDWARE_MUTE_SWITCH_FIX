# IQ200 OS v9.2-alpha16 AUTO NEXT FSM FIX

Fixes a stale one-shot completion suppression flag that caused the second natural EOF to be treated as a manual handoff stop.

Expected repeating sequence:

```text
[AUTONEXT] EOF queued
[HANDOFF][FSM] enter source=EOF ... suppress=0
[HANDOFF] NEXT selected
[HANDOFF] start OK
```

The sequence must repeat for track 2 -> 3 and later tracks.
