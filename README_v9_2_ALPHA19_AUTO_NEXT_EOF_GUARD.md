# IQ200 OS v9.2-alpha20 AUTO NEXT EOF GUARD

## Added
- Explicit natural-EOF classification in Core0 completion handling.
- FSM log when natural EOF overrides a stale suppression flag.

## Changed
- Natural EOF now has priority over `suppressRtCompletionOnce`.
- Auto Next remains deferred to the next Core0 cycle.

## Fixed
- A track ending naturally could be reported as `RT stopped` instead of `RT finished`.
- Auto Next could remain at `nextReq=0 nextAuto=0` after EOF.
- A stale manual-handoff suppression flag could block later automatic transitions.

## Disabled / removed
- Disabled the old suppression-first EOF classification.
- No Player UI, VU, FLAC decoder, SD timing, or soft-key behavior was removed.

## Expected EOF log
```text
[AUTONEXT][FSM] natural EOF overrides stale suppress; clearing   # only if stale
[CORE0] FLAC RT finished 100% ...
[AUTONEXT] EOF queued current=N/280
[HANDOFF][FSM] enter source=EOF ... suppress=0
[HANDOFF] NEXT selected: ...
[HANDOFF] start OK ...
```
