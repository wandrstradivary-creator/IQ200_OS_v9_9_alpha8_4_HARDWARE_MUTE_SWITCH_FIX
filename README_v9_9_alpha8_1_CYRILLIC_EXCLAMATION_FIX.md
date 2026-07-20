# IQ200 OS v9.9-alpha8.1 — Cyrillic exclamation mark fix

- Fixed the `!` glyph in the built-in 5x7 UTF-8/Cyrillic bitmap font.
- The glyph is now two pixels wide, so it remains clearly visible at scale 1 and scale 2.
- Applies to mixed Ukrainian/Cyrillic metadata rendered through `iqDrawText` and `iqDrawTextClipped`.
