# IQ200 OS v9.9-alpha7.9 — WebRadio Flicker Fix

The WebRadio base framebuffer is no longer pushed once per second. Artwork, VU, marquee text, volume and footer use independent bounded redraw regions, preventing the black-frame flash previously visible over artwork and VU.
