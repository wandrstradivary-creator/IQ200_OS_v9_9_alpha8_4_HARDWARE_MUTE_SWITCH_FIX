# IQ200 OS v9.3-alpha1 ARTWORK ENGINE

## Added
- Core0 album-art loader before audio decoder start.
- One-album PSRAM JPEG cache keyed by the track directory.
- One SD lookup/decode source load per album folder.
- Candidate names: folder/cover/front/album, JPG/JPEG with common case variants.
- 120x96 Player artwork area.
- Lightweight drawn CD DISC placeholder when art is missing or invalid.
- Runtime logs: `[ART] cache hit` and `[ART] album=... result=...`.

## Changed
- Track startup now prepares artwork before `IQPlayerCore::play()`.
- Player redraws artwork only when the cache generation changes.

## Disabled
- Repeated SD search for every track in the same album directory.
- Repeated JPEG file reads for the same album.
- Artwork SD access from Core1/UI.
- Artwork reads from the VU/progress render loop.

## Preserved
- v9.2-alpha20 Auto Next EOF Guard.
- FLAC RT path and SD cooldown.
- Segmented VU, Peak Hold and independent UI scheduler.

## Limits
- This first build supports external JPG/JPEG files. Embedded FLAC pictures and PNG are not decoded yet.
- JPEG files larger than 512 KiB use the CD DISC fallback.
