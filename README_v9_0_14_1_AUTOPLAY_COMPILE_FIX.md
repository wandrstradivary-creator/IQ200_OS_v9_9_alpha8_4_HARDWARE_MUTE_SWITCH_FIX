# IQ200 OS v9.0.15.1 AUTOPLAY COMPILE FIX

Fixes compile error: EVT_MEDIA was not declared.

Changed Autoplay start event to EVT_MONITOR, which exists in EventQueue.h.
Updated PlatformIO env to esp32s3-n16r8-iq200-os-v9015-autoplay-runtime-verify.
