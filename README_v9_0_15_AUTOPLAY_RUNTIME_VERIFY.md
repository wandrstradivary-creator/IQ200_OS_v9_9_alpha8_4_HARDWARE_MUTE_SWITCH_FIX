# IQ200 OS v9.0.15 — Autoplay Runtime Verify

Runtime verification/stabilization release after v9.0.14.1 compile fix.

## Focus

- Keep Track-only Resume policy: resume restores track only, never position.
- Verify Autoplay runtime path after SD/DB/Resume boot.
- Avoid double decoder start during boot.
- Add retry/cooldown when SD/DB/player are not ready at the first autoplay tick.
- Keep compile-safe EventBus usage (`EVT_MONITOR`).

## Commands

```text
autoplay
autoplay on
autoplay off
```

## Config

```text
/iq200/db/resume/autoplay.cfg
```

Example:

```text
# IQ200 AUTOPLAY v1
ENABLED=1
DELAY_MS=1800
```

## Expected boot log

```text
[AUTOPLAY] armed: delay=1800ms track=/...
[AUTOPLAY] start track-only from 0%: /...
```

If SD/DB/player are not ready yet:

```text
[AUTOPLAY] retry 1/6 status=WAIT_SD_DB next=1000ms track=/...
```

If a decoder is already active:

```text
[AUTOPLAY] retry 1/6 status=WAIT_BUSY next=750ms track=/...
```

## Test

```text
autoplay on
resume
reboot
autoplay
media
diag
stop
autoplay off
reboot
media
autoplay
```

## Notes

- Autoplay starts from 0% by design.
- Autoplay will not restore position.
- Autoplay will not start if the saved track is missing.
