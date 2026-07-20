# IQ200 OS v9.8-alpha34 WEB EQ SETTINGS

Adds Equalizer controls to Web UI Appearance page.

- Presets: flat, rock, pop, jazz, classic, bass, treble, vocal, off
- Custom Bass/Mid/Treble range: -12..+12 dB
- Current EQ values exposed in /api/status
- Uses existing Core1 command queue; no DSP work is performed in Web task
- EQ state continues to be saved by Smart Resume
