# IQ200 OS v9.8-alpha12 — Player UI Themes

## Added
- Four 480x320 Player 2.0 layouts: Artwork Focus, Dual VU, Modern, Classic Technical.
- Separate L/R VU rows in every layout.
- Artwork Focus is the default layout with 190x180 album artwork.
- Layout selection is saved in NVS namespace `iq200_player`.
- Web UI: Appearance -> Player UI 2.0 selector.
- Console commands:
  - `player theme artwork`
  - `player theme dual`
  - `player theme modern`
  - `player theme classic`
  - `player theme`

## Main coordinates (Artwork Focus)
- Artwork: x=16, y=54, w=190, h=180
- Title: x=222, y=62, w=242, h=34
- State: x=222, y=104, w=146, h=24
- Volume: x=370, y=104, w=94, h=24
- VU L: x=222, y=170, w=242, h=12
- VU R: x=222, y=188, w=242, h=12
- Progress: x=16, y=244, w=392, h=16
- Progress text: x=414, y=242, w=50, h=20
- Controls: y=282, h=30

## Test
1. Open Player.
2. Open Web UI -> Appearance.
3. Select each Player UI layout and press Apply layout.
4. Verify artwork, title, progress, dual VU, and controls remain fully inside 480x320.
5. Reboot and verify the selected layout is restored.
