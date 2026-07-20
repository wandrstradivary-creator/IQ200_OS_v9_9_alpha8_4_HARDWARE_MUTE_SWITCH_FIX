# IQ200 OS v9.2-alpha4 AUTO NEXT ONLY

## Scope

This build changes only automatic playback of the next playlist track after a confirmed natural decoder EOF.

## Behavior

- Natural EOF calls the existing atomic NEXT handoff directly.
- The next playlist item is selected.
- Playback of the selected item is started automatically.
- Manual NEXT/PREV behavior is unchanged.
- Scan scheduling and SD ownership logic are not modified by this release.

## Expected log

```text
[AUTONEXT] EOF current=1/280 -> next
[AUTONEXT] selected: /Music/... (2/280)
[HANDOFF] start OK codec=FLAC path=/Music/...
```

## Test

Start playback and allow the track to finish naturally. Do not issue NEXT or STOP. The next track must start automatically.
