# IQ200 OS v9.9-alpha7.13 WEBRADIO WEB PLAYER

- Added a dedicated WebRadio player card to the browser UI.
- Shows station artwork, station name, stream title, connection state and stereo VU.
- Added previous, play, stop and next station controls plus WebRadio volume.
- Status refresh remains bounded and all playback actions use asynchronous queues.

# IQ200 OS v9.9-alpha7.12 WEBRADIO STATION BROWSER

- Added a TFT station browser backed by the same NVS station store as Web UI.
- NAV encoder opens and scrolls the station list; NAV press selects and queues playback.
- Cyrillic station names use the existing UTF-8 clipped renderer.
- Selected station index is remembered across reboot.
- VU, artwork and marquee stop drawing while the browser overlay is open.

# IQ200 OS v9.9-alpha7.11 WEBRADIO REAL VU STYLES

- Replaced the cosmetic VU variants with six distinct renderers.
- Line is a continuous analogue rail; Thin uses vertical needles.
- Blocks uses wide LED cells; Dots uses circular LEDs.
- Neon adds a bounded glow/core effect; Center grows symmetrically outward.
- Rendering remains inside the existing 440×48 partial PSRAM sprite.

# IQ200 OS v9.9-alpha7.10 WEBRADIO VU STYLE SWITCH FIX

- Fixed the Web UI status poll resetting the VU style selector before Apply.
- VU style changes are now submitted immediately on selection.
- Added a three-second pending state until `/api/status` confirms the new style.
- Preserved audio-first partial VU rendering and all alpha7.9 flicker fixes.

# IQ200 OS v9.9-alpha7.9 WEBRADIO FLICKER FIX

- Removed the unconditional one-second full-frame TFT refresh in WebRadio.
- StreamTitle marquee, VU, artwork, volume and footer now update as independent regions.
- Volume changes no longer push the 480x320 framebuffer over artwork/VU.
- Footer hint expiration repaints only the 34-pixel footer.
- Full-frame present is reserved for real station/state/theme changes.

# IQ200 OS v9.9-alpha7.8 WEBRADIO VISUALS

- Added Neon and Center VU variants alongside Line, Thin, Blocks and Dots.
- Added persistent VU level gain (50–300%) and noise gate (0–30).
- Added station artwork URL to NVS storage, Web editor, JSON import/export and station cards.
- Added a 256 KB PSRAM JPEG/PNG cover cache with strict connection/read timeouts.
- Cover download completes before the new audio stream starts, so it never competes with active playback.
- Added independent 10 FPS marquee scrolling for long UTF-8 station and StreamTitle rows.
- Retained the exact ESP32-audioI2S 2.0.7 VU callback and Cyrillic renderer.

# IQ200 OS v9.9-alpha7.7 WEBRADIO CYRILLIC

- Added UTF-8 Cyrillic rendering for WebRadio station names and StreamTitle metadata on TFT.
- Reuses the existing compact Ukrainian bitmap font with А–Я, а–я, І, Ї, Є and Ґ.
- Keeps the native LovyanGFX font for ASCII-only station metadata.
- Clips long metadata to the 440-pixel content area so it cannot overwrite volume or VU widgets.
- Added common punctuation glyphs used in station and track names.

# IQ200 OS v9.9-alpha7.6 WEBRADIO VU 2.0.7 FIX

- Fixed WebRadio VU staying at zero with the pinned `esphome/ESP32-audioI2S 2.0.7` package.
- Uses its exact `audio_process_i2s(uint32_t*, bool*)` callback signature.
- Unpacks post-EQ/post-volume left and right 16-bit samples from the library's 32-bit stereo frame.
- Sets `continueI2S=true` so normal PCM output continues to the DAC.
- Keeps the alpha7.5 WebRadio Web settings, EQ, volume, theme and VU configuration.

# IQ200 OS v9.9-alpha7.5 WEBRADIO WEB SETTINGS

- Enabled the Appearance tab in WebRadio clean-boot mode.
- Added persistent WebRadio theme, brightness, volume, VU style/segments/FPS/peak/hold/decay controls.
- Added persistent WebRadio EQ presets and custom three-band tone control.
- EQ changes are queued and applied only by `webradio_rt`; Web handlers never call the decoder directly.
- Replaced the unavailable `getVUlevel()` call with the ESP32-audioI2S 2.0.7 `audio_process_i2s` PCM callback.
- Kept Local-only layout, SD clock and navigation cards hidden in WebRadio mode.

# IQ200 OS v9.9-alpha7.4 WEBRADIO VU

- Added independent stereo L/R VU meters to the WebRadio TFT screen.
- Samples the decoder's packed channel levels in `webradio_rt` at 40 Hz with fast attack and bounded release.
- Updates only a 440x48 PSRAM sprite at 20 FPS; the full 480x320 framebuffer is not redrawn for VU animation.
- Added peak hold markers and green/yellow/red 24-segment scales.
- Resets VU immediately on stop, station change and unsupported-stream failure.
- Exposes `radioVuL`, `radioVuR` and `radioVuTicks` in `/api/status` for diagnostics.

# IQ200 OS v9.9-alpha7.3 UNSUPPORTED STREAM GUARD

- Stops unsupported OGG/Opus/Ogg-FLAC sessions in a terminal `ERROR` state.
- Prevents the normal EOF/stall recovery path from reconnecting forever to an unsupported stream.
- Exposes unsupported-stream count through status and WebRadio REST diagnostics.
- Retains working MP3/AAC HTTP/HTTPS playback, redirects, metadata, volume and responsive Web control.

# IQ200 OS v9.9-alpha7.2 MODE UI FRAMEBUFFER FIX

- Mode Center and WebRadio now use the same PSRAM RGB565 framebuffer/present path as the proven Local Player UI.
- Every full mode frame is pushed inside an explicit TFT `startWrite()` / `endWrite()` transaction.
- Direct TFT rendering remains only as a low-memory fallback when the 480x320 framebuffer cannot be allocated.
- Added framebuffer allocation diagnostics for both lightweight mode UIs.

# IQ200 OS v9.9-alpha7.1 MODE CENTER FIRST FRAME FIX

- Fixed a blank display after successful Mode Center boot.
- The initial Mode Center frame is now rendered synchronously immediately after `display.begin()`.
- The FreeRTOS UI task now handles only encoder input and subsequent redraws.
- Applied the same first-frame ordering to the lightweight WebRadio screen.
- Added `[MODE_UI] first frame begin/ready` serial diagnostics.

# IQ200 OS v9.9-alpha7 MODE CENTER CLEAN BOOT

- Added an NVS-backed Mode Center with isolated Local Player and WebRadio boots.
- Mode Center starts only the display/input selector; WiFi, SD, Web and audio remain off.
- Local Player no longer creates the ESP32-audioI2S/WebRadio task.
- WebRadio no longer initializes SD, media DB, local decoders, Resume, EQ or Artwork.
- ESP32-audioI2S is allocated lazily only when WebRadio is selected.
- Mode changes save the target, stop the active platform and perform a full reboot.
- Added 10-second boot-health confirmation and fallback to Mode Center after three early failures.
- Holding NAV during reset opens Mode Center; holding both encoder buttons for two seconds returns to it.
- Added mode-aware REST guards and Web UI platform visibility.
- Bluetooth and FM/Radio are visible as disabled future modes.

# IQ200 OS v9.8-alpha34 WEB EQ SETTINGS

- Added Web Appearance Equalizer controls.
- Added EQ preset selector and Bass/Mid/Treble custom sliders.
- Added EQ state to /api/status.
- Uses existing command queue and Smart Resume persistence.

# v9.8-alpha29 RGB565 Artwork Fit
- Fixed tiny TFT artwork by decoding to RGB565 in PSRAM and resizing to the full fit-center box.

# IQ200 OS v9.8-alpha26 FLAC RT TASK MEMORY FIX

- Fixed xTaskCreatePinnedToCore failure at FLAC pipeline S11.
- Moved non-critical stereo helper buffer to PSRAM-first allocation.
- Added adaptive FLAC task stack: 12 KB, then 8 KB fallback.
- Added largest internal heap block diagnostics.

# IQ200 OS v9.8-alpha25 PLAYBACK PIPELINE AUDIT

- Numbered FLAC startup steps S1..S15.
- Exact I2S install/pin/clock errors.
- Heap and PSRAM checkpoints.
- First decode and first I2S write diagnostics.
- Safe one-shot stale I2S driver recovery.

## v9.8-alpha23 POST-SCAN PLAY FIX

- Restore player to READY after successful scan.
- Clear stale audio, handoff and navigation flags.
- Reset progress/VU mirrors after playlist reload.
- Add post-scan player finalization log.

## v9.8-alpha22 ARTWORK GENERATION LOCK
- Removed force-driven artwork redraw from player partial updates.
- Volume, STOP, state, VU, progress and marquee cannot repaint album art.
- Artwork redraw now occurs only when ArtworkCache generation changes.

# v9.8-alpha17 ARTWORK FIT CENTER
- Fixed album image showing only the top-left fragment.
- Added proportional fit-center scaling for JPEG/PNG.
- Preserved aspect ratio and centered output.

# IQ200 OS v9.8-alpha13 — SD Concurrency Guard

- Blocks Artwork, Resume, Queue and DB writes while audio streaming or SD recovery is active.
- Adds an explicit SD recovery state around the complete 8 MHz bridge / 12 MHz remount window.
- Resume save/load now acquires the central recursive SD mutex and re-checks ownership after locking.
- Artwork reads are limited to 512 KB, performed in 4 KB chunks, and yield/reset WDT between chunks.
- Prevents `ArtworkCache::prepareForTrack()` IDLE0 watchdog stalls from one giant `File::read()`.
- Prevents `ResumeEngine::save()` from opening a VFS file while `SD.end()/SD.begin()` invalidates the filesystem.
- Preserves Player UI themes, Web Music Browser, WiFi Boot Manager and 12→8→12 MHz recovery bridge.

# v9.8-alpha11 SD Recovery Bridge

- 12 MHz remains the only sustained playback clock.
- 8 MHz is used only for a short recovery remount probe.
- Recovery restores 12 MHz before reopen, seek, prefetch, and decoder resume.
- Partial failed prefetch data remains hidden from decoders.

# v9.8-alpha10 WEB MUSIC BROWSER
- Added paginated Web track list, search and direct playback.

## v9.5-alpha2.1 — Progress Widget 2.1
- Cached progress percentage text; redraw only on integer-percent change.
- Corrected UI profiler FPS to count full and partial renderer frames.
- Audio core unchanged.

# v9.3-alpha2.7 SD RECOVERY + PREFETCH

## Added
- 128 KB compressed prefetch cache.
- 8 KB physical SD read chunks.
- SD remount/reopen/position recovery.
- recovery/fatal statistics.

## Changed
- SD starts at 16 MHz.
- FLAC decoder block reduced to 4096 frames.

## Disabled
- 20 MHz SD mode.
- immediate SDERR on a transient read failure.

## v9.4-alpha1 — Black Box + Performance Dashboard

- Added 512-entry PSRAM Black Box recorder with DRAM fallback.
- Added `bb on/off/status/clear/dump` and `bb dump <count>`.
- Added `perf` compact runtime dashboard.
- Excluded VU and progress traffic from Black Box to protect RT performance.
- Retained stable v9.3-alpha2.7 SD recovery/prefetch audio core unchanged.

## v9.4-alpha4 — VU LINE ENGINE
- Added `vu style line`.
- Uses one-pixel `drawFastVLine()` segments for minimal UI cost.
- Persists with existing VU NVS settings.
- Audio and SD cores unchanged.

## v9.5-alpha1 — UI FOUNDATION
- Added centralized `PlayerLayout` geometry.
- Converted Player full and partial render coordinates to layout rectangles.
- Added per-widget UI profiler and `ui/status/reset` commands.
- Kept Audio Core, SD transport, Auto Next, themes, VU, and Black Box unchanged.

## v9.5-alpha2 — PROGRESS WIDGET 2.0

- Progress bar now uses delta-only TFT updates.
- Forward playback paints only newly completed pixels.
- Seek/restart clears only the obsolete pixel strip.
- Static frame is redrawn only after Player cache reset/theme refresh.
- Percentage text remains synchronized at 1 Hz.
- Audio Core, SD prefetch/recovery, Auto Next and FLAC decoder are unchanged.

## v9.8-alpha5 — Navigation / AutoNext FSM Fix
- Manual Next/Prev cancels queued EOF AutoNext.
- Controlled Navigation STOP has priority over progress-based EOF detection.
- AutoNext is blocked during Navigation preview and commit.
- Prevents duplicate playback starts during rapid Web navigation.


## v9.8-alpha6 — SD Recovery 12M→8M
- Conservative 12/8 MHz governor profile.
- Fixed invalid File/seek early-return that bypassed recovery.
- Added remount/reopen/seek continuation diagnostics.

## v9.8-alpha7 — Artwork LRU PSRAM
- Replaced one-album artwork cache with four-entry PSRAM LRU cache.
- Added generic JPG/JPEG/PNG fallback lookup.
- Added art info/cache/clear/reload commands.
- Artwork reload is queued to Core0 to preserve SD ownership.


## v9.8-alpha8
- WiFi Boot Manager: saved-profile AutoConnect, AutoReconnect, fallback AP, persistent policy, Web/CLI controls.


## v9.8-alpha9 CAPTIVE PORTAL
- Wildcard DNS redirect while AP/AP+STA is active.
- Captive portal probe endpoints for Android, iOS, Windows and Firefox.
- Unknown HTTP routes redirect to 192.168.4.1 only while AP is active.
- WiFi Boot Manager, AutoConnect and Fallback AP retained.

## v9.8-alpha12 PLAYER UI THEMES
- Redesigned 480x320 Player screen around album artwork.
- Added Artwork Focus, Dual VU, Modern and Classic layouts.
- Added separate L/R VU rows.
- Added Web Appearance layout selector and NVS persistence.

## v9.8-alpha15
- Large track title in Player Artwork Focus layout.
- Smooth UTF-8 horizontal marquee with endpoint pauses.
- Title-only partial repaint to avoid unnecessary metadata redraw.

## v9.8-alpha16
- Reduced title marquee refresh from ~18 FPS to 4 FPS.
- Added audio-health priority guard and automatic marquee cooldown.
- Increased endpoint pause to 2 seconds while preserving large title layout.

## v9.8-alpha20 Theme Pack 1.0
- Added eight persistent themes.
- Added Gold Peak Hold.
- Added four-zone VU colors and Web theme controls.

## v9.8-alpha20 LATIN FONT FIX
- Added missing uppercase Latin glyph L to Font5x7.
- Added G, J, K, Q, X and Z to prevent fallback boxes in track titles.

## v9.8-alpha21 VOLUME PARTIAL REDRAW
- Volume updates only the 50x22 volume region on the Player screen.
- Removed forced full player partial refresh from encoder and command volume paths.
- Artwork is never redrawn on volume-only changes.

## v9.8-alpha31 CYRILLIC RESUME EQ
- Fixed Cyrillic Ц/ц glyphs.
- Current track is committed to resume.dat before opening audio stream.
- Added low-resource shared 3-band DSP EQ and presets.
