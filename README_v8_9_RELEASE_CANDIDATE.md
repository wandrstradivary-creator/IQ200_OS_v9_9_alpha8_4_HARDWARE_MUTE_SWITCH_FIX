# IQ200 OS v8.9 — Release Candidate

Purpose: RC build after v8.8 Commercial Polish. No new feature expansion; only bugfix, version cleanup, diagnostics polish and stability guard tuning.

Changes:
- Version/env updated to v8.9_RELEASE_CANDIDATE.
- Stability/Diagnostics/Web/UI banners aligned to v8.9.
- Stability leak warning tuned to reduce false positives during startup/SD/Wi-Fi churn.
- All v8.7.2 recovery fixes preserved: no file descriptor exhaustion, Cyrillic search, scan-safe stability/burnin commands.
- All v8.8 polish features preserved.

Recommended tests:
```
diag
stability
polish
find цой
burnin start
scan
stability
burnin stop
find виктор
```

If this build passes, next step is v9.0 IQ200 OS Enterprise Stable.
