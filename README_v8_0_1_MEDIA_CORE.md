# IQ200 OS v8.0.1 MEDIA CORE X10THINK

Base: v7.7.1 FIND UI POLISH

Added:
- `src/services/MediaCore.h`
- `src/services/QueueManager.h`
- queue foundation without touching RT Audio
- `/iq200/db/queue.db` save/load support
- commands:
  - `queue` / `q`
  - `qadd`
  - `qclear`
  - `qnext`
  - `qprev`
  - `qsave`
  - `qload`
- `RuntimeState` queue bridge fields
- boot banner updated to v8.0.1

Notes:
- RT Audio path is unchanged.
- WAV/MP3/FLAC scan/database engine is unchanged.
- QueueManager is a foundation layer for future Smart Resume and Now Playing queue.
