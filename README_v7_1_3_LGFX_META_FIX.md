# IQ200 OS v7.1.3 LGFX META FIX

Fix compile error in `src/ui/UI.h`:

- removed unsupported `LGFX_Sprite::getTextBounds()`
- replaced with `LGFX_Sprite::textWidth()`
- keeps compact centered audio metadata: `44.1 kHz • Stereo`

Reason: LovyanGFX `LGFX_Sprite` does not provide `getTextBounds()`.
