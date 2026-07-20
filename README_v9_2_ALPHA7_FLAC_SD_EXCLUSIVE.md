# IQ200 OS v9.2-alpha7 FLAC SD EXCLUSIVE

## Fix

Runtime logs showed FLAC decoder reads colliding with Library Search and SmartResume writes.

During active FLAC playback:

- SmartResume autosave is postponed without clearing dirty state.
- `find/search` does not open `search.idx`; it reports that FLAC owns SD.
- FLAC decode block remains 4096 PCM frames with internal-RAM-first allocation.

After playback stops, SmartResume can save normally. Run search again after `stop`.

Expected protection log:

```text
[FIND] blocked during FLAC playback query='rox'; stop playback and retry
```
