# IQ200 OS v7.3.3 RECURSIVE MEDIA SCAN X10THINK

Base: v7.3.2 Smart Playlist.

## Added
- Recursive SD card scan across all folders, not only root.
- Playlist capacity increased from 32 to 256 tracks.
- FileIndex capacity increased from 32 to 256 files.
- Adds all supported media files to playlist: `.mp3`, `.flac`, `.wav`.
- Priority playlist order: MP3 -> FLAC -> WAV.
- Playlist list output now shows codec tag: `[MP3]`, `[FLAC]`, `[WAV]`.
- Longer media path buffers: 192 bytes for nested folders and long filenames.
- Play guard for unsupported current codecs:
  - MP3: prints `MP3 decoder not ready`, no WAV open attempt.
  - FLAC: prints `FLAC decoder not ready`, no WAV open attempt.
  - WAV: uses existing stable RT Audio path.

## Commands
- `scan` / `plscan`: recursive SD scan and playlist build.
- `list` / `pl`: show playlist with current marker and codec.
- `next` / `prev`: choose current playlist item.
- `play` / `p`: play current item if codec is implemented.

## Notes
RT Audio path for WAV was not changed.
MP3/FLAC are prioritized and listed, but decoders are still placeholders for a future version.
