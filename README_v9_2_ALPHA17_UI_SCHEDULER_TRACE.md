# IQ200 OS v9.2-alpha17 UI SCHEDULER TRACE

- VU scheduler: 20 ticks/s.
- Progress scheduler: 1 tick/s.
- LCD draw counters are separate from scheduler ticks.
- `[PROC][C1] vu=tick/draw prog=tick/draw meta=draw` reports true rates per second.
- Low draw rate is normal when delta rendering detects no visual change.
- Auto Next FSM fix from alpha16 is preserved.
