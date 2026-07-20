# IQ200 OS v9.3-alpha2.1 — dr_flac API compatibility fix

## Fixed
- Added the `drflac_tell_proc` callback required by the installed dr_flac version.
- Removed dependency on the unavailable `drflac_seek_origin_current` symbol.
- Kept FLAC reads and seeks routed through `SDManager::Stream`.

## Disabled / removed
- The incompatible four-argument `drflac_open()` call.
- Direct use of the missing seek-origin enum constant.

## Unchanged
- SD stream transport, retry counters, Artwork cache, Auto Next, VU and UI scheduler.
