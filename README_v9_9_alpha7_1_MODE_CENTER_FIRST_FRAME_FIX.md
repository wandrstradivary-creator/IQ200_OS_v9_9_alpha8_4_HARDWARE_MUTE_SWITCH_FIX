# IQ200 OS v9.9-alpha7.1 — Mode Center First Frame Fix

This maintenance build fixes the blank TFT observed when Mode Center reported a successful display initialization and task creation.

After `display.begin()`, the ILI9488 receives a 120 ms settling interval and the first Mode Center frame is rendered synchronously in `setup()`. Only after the frame is visible is `mode_center_ui` created for encoder input and later redraws. WebRadio uses the same safe first-frame order.

Expected serial sequence:

```text
Display init: 1
[MODE_UI] first frame begin
[MODE_UI] first frame ready
[TASK] mode_center_ui create=OK stack=6144
[TASK] modeCenterTask running on core 1
```

All Mode Center clean-boot isolation, Resume position, dedicated Web task, Artwork guard and WebRadio asynchronous playback changes from alpha7 are retained.
