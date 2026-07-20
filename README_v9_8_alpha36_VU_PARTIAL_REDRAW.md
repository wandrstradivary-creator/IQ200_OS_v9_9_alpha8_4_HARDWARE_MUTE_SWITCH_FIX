# IQ200 OS v9.8-alpha36 VU PARTIAL REDRAW

- VU PRO changes repaint only the two TFT VU rows.
- Artwork, title, progress, time, buttons and background are not redrawn.
- Applies to style, segment count, FPS, peak, hold and decay commands from Web or console.
- Web VU settings remain available.
- Expected log: `[UI] partial redraw=VU reason=...`
