# v8.2.5 — Smart Resume

Added SmartResumeService for automatic resume state management on SD.

Persistent files:

- `/iq200/db/resume/resume.dat`
- `/iq200/db/queue/queue.db`

Saved state:

- track path/title/codec/state;
- played bytes and progress percent;
- volume;
- queue current/index/count;
- shuffle/repeat.

Runtime behavior:

- boot auto-restore after SD mount and DB start;
- queue auto-load;
- debounced autosave;
- explicit resume commands preserved.
