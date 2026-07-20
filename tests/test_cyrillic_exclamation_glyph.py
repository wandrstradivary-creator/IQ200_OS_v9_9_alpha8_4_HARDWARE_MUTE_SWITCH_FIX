from pathlib import Path

FONT = Path(__file__).parents[1] / "src" / "ui" / "Font5x7.h"

def test_exclamation_glyph_is_present_and_visible():
    text = FONT.read_text(encoding="utf-8")
    assert "{U'!',{0,0x5F,0x5F,0,0}}" in text

def test_utf8_text_uses_same_ascii_punctuation_table():
    text = FONT.read_text(encoding="utf-8")
    assert "char32_t c = iqNextUtf8(p);" in text
    assert "iqGlyph(c)" in text
