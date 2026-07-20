# IQ200 OS v8.2.3 — Partial Renderer X10THINK

Goal: reduce UI load during playback and async work by repainting only dynamic rectangles.

Implemented:
- RendererStats now tracks full frames and partial frames separately.
- RuntimeState exposes rendererFrames/dirtyFrames/fullFrames/partialFrames.
- Player screen keeps the static layout in the PSRAM framebuffer.
- High-frequency widgets are updated directly on ILI9488:
  - progress bar and percent,
  - VU left/right bars,
  - volume value and bar,
  - PLAYING/STOPPED/READY status,
  - buffer/underrun/I2S health line.
- Core events no longer force full `playerScreen()` redraw while playing.
- Full redraw still happens on screen entry, navigation, and static screen changes.

Next: v8.2.4 Database Service.
