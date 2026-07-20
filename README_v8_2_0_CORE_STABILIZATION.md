# IQ200 OS v8.2.0 CORE STABILIZATION X10THINK

Added a stabilization layer without touching RT Audio.

## Changes
- Version banner updated to `v8.2.0 CORE STABILIZATION`.
- `diag / diagnostics` command.
- Diagnostics screen shows heap, stack, audio underruns, DB counts, pipeline queue depth, event counters, scan state, partial-render policy, boot timing and service heartbeat.
- `ServiceManager` now mirrors heartbeat/watchdog counters into `RuntimeState`.
- Boot phase and boot ready time are tracked.
- Scan lock marks scanner busy for service diagnostics.

## Not changed
- RT Audio path.
- WAV player task.
- I2S configuration.
- MediaDatabase file format.
