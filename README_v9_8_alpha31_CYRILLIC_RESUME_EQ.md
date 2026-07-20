# IQ200 OS v9.8-alpha31 CYRILLIC RESUME EQ

- Added correct Cyrillic glyphs Ц/ц to Font5x7.
- Commits current selected track to resume.dat immediately before decoder stream opens.
- Added low-resource 3-band DSP equalizer shared by MP3/FLAC/WAV.
- Presets: flat, rock, pop, jazz, classic, bass, treble, vocal, off.
- Custom command: `eq custom <bass -12..12> <mid -12..12> <treble -12..12>`.
- EQ state is stored in resume.dat.

Test: `eq rock`, `eq`, play a track, reboot and verify current track + EQ restore.
