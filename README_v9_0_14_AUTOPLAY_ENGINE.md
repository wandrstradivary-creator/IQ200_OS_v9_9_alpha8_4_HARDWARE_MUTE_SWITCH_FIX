# IQ200 OS v9.0.15 — AUTOPLAY_ENGINE

X10THINK incremental build after v9.0.13 Playback Engine Final.

## Goal

Add optional boot autoplay while keeping the user-requested track-only resume policy:

- Resume stores/restores only the track, playlist index, volume, repeat/shuffle.
- Playback always starts from 0%; no position/progress resume.
- Autoplay is optional and persisted on SD.

## New commands

```text
autoplay
autoplay status
autoplay on
autoplay off
```

Aliases:

```text
aplay
aplay on
aplay off
```

## Runtime behavior

Boot sequence:

```text
Mount SD
Load media DB
Load SmartResume track-only target
Load /iq200/db/resume/autoplay.cfg
Wait delay
Start current track from 0%
```

Autoplay starts only if:

- SD is mounted;
- DB has tracks;
- resume/current track exists in runtime state;
- decoder/audio task is not already busy;
- autoplay is enabled.

## SD config

```text
/iq200/db/resume/autoplay.cfg
```

Format:

```text
# IQ200 AUTOPLAY v1
ENABLED=1
DELAY_MS=1800
```

## Notes

This does not add new codecs and does not change FLAC/WAV playback paths.
It only coordinates boot playback after the stable DB/resume initialization path.
