# IQ200 OS v8.0.2 EVENT BUS RESUME X10THINK

Added:
- `EventBus.h` over the existing MessageBroker/FreeRTOS event queue.
- Event counters in RuntimeState.
- `ResumeEngine.h` with `/iq200/db/resume.dat`.
- Serial commands: `resume`, `rsave`, `rload`, `rclear`.
- Help and startup banners updated to v8.0.2.

Notes:
- RT Audio path was not changed.
- Resume writes only on explicit command in this build to avoid SD write wear.
- Future v8.0.3 can add timed/autosave and incremental library update.
