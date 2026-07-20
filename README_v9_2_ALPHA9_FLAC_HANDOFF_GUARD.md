# IQ200 OS v9.2-alpha9 FLAC HANDOFF GUARD

## Purpose
Protect SD/FATFS from rapid FLAC close/open cycles during repeated NEXT/PREV.

## Changes
- 150 ms SD settle delay after the active decoder task fully stops.
- 600 ms debounce for repeated user NEXT/PREV commands.
- EOF Auto Next is not blocked by the user-command debounce.
- FLAC 4096-frame decode block, SD-exclusive background policy and low-resource Player UI are preserved.

## Expected log
```
[HANDOFF] NEXT: stopping current decoder
[AUDIO-RT] FLAC task stopped state=STOP ...
[HANDOFF] NEXT selected: ...
```
Rapid repeated input may print:
```
[HANDOFF] NEXT ignored: SD cooldown 245/600 ms
```
EOF Auto Next continues normally.
