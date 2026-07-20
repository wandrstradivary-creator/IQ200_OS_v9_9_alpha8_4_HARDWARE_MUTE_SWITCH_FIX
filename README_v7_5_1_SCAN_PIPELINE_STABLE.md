# IQ200 OS v7.5.1 SCAN PIPELINE STABLE X10THINK

- Single-pass SD scan залишено: MP3 / FLAC / WAV / ART за один прохід.
- Виправлено статистику: `Files` тепер показує реальну кількість файлів, а не кількість yield-ітерацій.
- `files/sec` став коректнішим.
- WDT-safe `vTaskDelay(1)` залишено, але він більше не спотворює лічильник файлів.
- Оновлено banner/help/footer до v7.5.1.
- RT Audio не змінювався.
