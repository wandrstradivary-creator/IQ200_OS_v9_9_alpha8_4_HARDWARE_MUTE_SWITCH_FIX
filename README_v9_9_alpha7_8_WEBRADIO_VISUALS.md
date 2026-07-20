# IQ200 OS v9.9-alpha7.8 — WebRadio Visuals

- VU styles: Line, Thin, Blocks, Dots, Neon and Center.
- VU gain: 50–300%; noise gate: 0–30.
- Station covers: optional JPEG/PNG URL, maximum 256 KB.
- Cover traffic is completed before `connecttohost()`, never during active streaming.
- Long station and track names scroll at a capped 10 FPS in a dedicated 340x18 sprite.
- Cyrillic and Ukrainian metadata remain supported.

Add a cover in WebRadio → Station Editor → Station cover URL. Images larger than 256 KB or slower than the bounded timeout are skipped; radio playback still starts.
