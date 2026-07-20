# IQ200 OS v9.9-alpha7.2 — Mode UI Framebuffer Fix

The Local Player log confirmed that the stable display path is its 480x320 RGB565 framebuffer in PSRAM followed by an explicit `startWrite()`, `pushSprite()` and `endWrite()` transaction.

Mode Center and the lightweight WebRadio screen now use that same path. Direct TFT drawing is retained only as an allocation-failure fallback.

Expected Mode Center diagnostics:

```text
Display init: 1
[MODE_UI] first frame begin
[MODE_UI] framebuffer=PSRAM_OK bytes=307200
[MODE_UI] first frame ready
```

This build retains clean platform isolation, NVS mode selection, boot recovery, Resume list-position fixes, asynchronous WebRadio and the dedicated Web task.
