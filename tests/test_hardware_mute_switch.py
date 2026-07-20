from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CPP = (ROOT / "src/services/RadioService.cpp").read_text()
PINS = (ROOT / "include/iq200_pins.h").read_text()

def test_active_low_mute_pin_defined():
    assert "#define MUTE_PIN             4" in PINS
    assert "#define MUTE_VAL             LOW" in PINS

def test_switch_and_reconnect_are_muted():
    assert "void RadioService::handlePlay" in CPP
    assert "setHardwareMute(true);" in CPP
    assert "scheduleHardwareUnmute(120);" in CPP
    assert "void RadioService::onEof" in CPP

def test_unmute_is_non_blocking_and_requires_playback():
    assert "unmuteAtMs = millis() + delayMs" in CPP
    assert "radio->isRunning() && state == PLAYING" in CPP
    assert "delay(120)" not in CPP
