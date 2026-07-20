# IQ200 OS v9.2-alpha3 AUTO NEXT + SCAN PRIORITY

## Behaviour

- Natural decoder EOF automatically selects and starts the next playlist track.
- Manual `next` keeps its existing handoff behaviour.
- A deferred full SD scan has priority at a natural track boundary.
- When `scan` is requested during playback, the current track continues.
- At EOF, Auto Next is skipped once and the deferred scan starts.

## Expected log

Normal EOF:

```
[AUTONEXT] queued after EOF current=1/280
[HANDOFF] NEXT selected: ... autoplay=1 source=EOF
```

Deferred scan at EOF:

```
[AUTONEXT] skipped: deferred scan has priority at EOF
[SCANSVC] playback idle: starting deferred SD scan
```
