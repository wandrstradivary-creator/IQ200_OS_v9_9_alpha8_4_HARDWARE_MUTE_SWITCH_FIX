# IQ200 OS v9.8-alpha30 Artwork Resolver

- Unified artwork path resolver for TFT and Web PSRAM cache.
- Exact names: cover/folder/front/album/artwork plus Обложка and Обкладинка.
- Exact track-basename image lookup.
- Directory candidate scoring by cover keywords and track-name overlap.
- A single image in an album folder is accepted automatically.
- Multiple unrelated images are rejected to avoid showing the wrong cover.
- Existing alpha29.1 RGB565 renderer and alpha26 audio pipeline are preserved.
