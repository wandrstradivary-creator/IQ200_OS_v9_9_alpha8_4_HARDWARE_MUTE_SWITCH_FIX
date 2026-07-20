# v8.7 — Stability RC

Focus: reliability before commercial polish.

Implemented:

- `StabilityService.h`
- Non-blocking tick-based supervisor.
- Burn-in control through serial commands.
- SD health probe with latency tracking.
- Heap/PSRAM minimum counters.
- Watchdog age warning counters.
- Recovery/error counters.
- `diag` includes Stability RC status.
- `stability` / `stab` command prints a dedicated report.

Serial commands:

```text
stability
stab
burnin start
burnin stop
```

Runtime metrics:

```text
stabilityStatus
burninActive
stabilityUptimeMs
stabilityTicks
stabilityRecoveries
stabilityWatchdogWarnings
stabilitySdErrors
stabilitySdLatencyMaxMs
stabilityMinHeap
stabilityMinPsram
stabilityLeakWarnings
```

This version is intended for repeated scan/play/stop/web/diagnostics testing before v8.9 Release Candidate.
