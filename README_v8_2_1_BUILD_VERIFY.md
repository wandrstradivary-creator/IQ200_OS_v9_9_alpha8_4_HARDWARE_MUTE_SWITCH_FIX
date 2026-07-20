# IQ200 OS v8.2.1 — Build Verify

Base: v8.2.0 Core Stabilization.

## Fixed
- Updated PlatformIO env to `esp32s3-n16r8-iq200-os-v821-build-verify`.
- Updated `IQ200_VERSION` to `8.2.1_BUILD_VERIFY`.
- Removed early `MediaDatabase::begin()` from setup before SD mount.
- Added safe DB begin guard when SD is not mounted.
- Added robust SD mount logging and `/iq200`, `/iq200/db`, `/iq200/config` creation after mount.
- Removed automatic `/test.wav` injection from the production playlist at boot.
- Updated serial/UI banners to v8.2.1.
- Kept the proven v8.x WAV/MP3/FLAC database path intact for build verification.

## Hardware target
ESP32-S3-WROOM-1-N16R8 + ILI9488 SPI + PCM5102 I2S + SD on SPI CS=38.

## Next
v8.2.2 will split scanning into a dedicated ScanService task so UI/encoder remain responsive during database rebuild.
