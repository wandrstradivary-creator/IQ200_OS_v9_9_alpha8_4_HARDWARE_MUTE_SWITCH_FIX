# IQ200 OS v9.8-alpha35 WEB EQ SYNC FIX

- Prevents 500 ms status polling from resetting EQ controls while editing.
- Keeps optimistic pending EQ values until RuntimeState confirms them.
- Preset and custom slider changes remain visible while the command queue processes them.
- Existing EQ DSP, Resume and Web settings are preserved.
