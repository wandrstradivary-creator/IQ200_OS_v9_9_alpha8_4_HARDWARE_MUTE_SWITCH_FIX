# IQ200 OS v7.3.4 WDT SAFE MEDIA SCAN

Fix for ESP32-S3 task watchdog during recursive SD scan.

Changes:
- FileIndex recursive scan now calls vTaskDelay(1) every few entries.
- Max scan depth limited to 6.
- Skips heavy/system folders: System Volume Information, LOST.DIR, .Trash, Android/data, Android/obb.
- Keeps playlist priority: MP3 -> FLAC -> WAV.
- RT Audio unchanged.

Reason:
- v7.3.3 could keep Core0 busy inside SD/FAT openNextFile/stat for too long.
- IDLE0 could not feed WDT, causing reset.
