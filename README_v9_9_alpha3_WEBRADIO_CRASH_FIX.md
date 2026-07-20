# IQ200 OS v9.9-alpha3 WebRadio Crash Fix

- Guards ESP32-audioI2S `stopSong()` until `connecttohost()` has created a valid radio session.
- Prevents the LoadProhibited panic in `i2s_zero_dma_buffer()` on first WebRadio Play.
- Removes only obsolete, existing station keys from NVS; no `sXX NOT_FOUND` flood.
- Corrects firmware banner and PlatformIO environment version.

Test: boot, import/replace station list, press Play, switch station, Stop, then play local FLAC.
