from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]


def block_after(source: str, marker: str) -> str:
    start = source.index(marker)
    brace = source.index("{", start)
    depth = 0
    for pos in range(brace, len(source)):
        if source[pos] == "{":
            depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0:
                return source[brace : pos + 1]
    raise AssertionError(f"unterminated block: {marker}")


class ModeCenterCleanBootTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.main = (ROOT / "src/main.cpp").read_text()
        cls.mode = (ROOT / "src/services/ModeManager.h").read_text()
        cls.web = (ROOT / "src/services/WebServerService.h").read_text()
        cls.radio_h = (ROOT / "src/services/RadioService.h").read_text()
        cls.radio_cpp = (ROOT / "src/services/RadioService.cpp").read_text()
        cls.center_ui = (ROOT / "src/ui/ModeCenterUI.h").read_text()
        cls.radio_ui = (ROOT / "src/ui/WebRadioModeUI.h").read_text()

    def test_modes_are_explicit_and_future_modes_are_not_bootable(self):
        for value in (
            "IQ200_MODE_CENTER = 0",
            "IQ200_MODE_LOCAL_PLAYER = 1",
            "IQ200_MODE_WEBRADIO = 2",
            "IQ200_MODE_BLUETOOTH = 3",
            "IQ200_MODE_RADIO = 4",
        ):
            self.assertIn(value, self.mode)
        available = block_after(self.mode, "static bool available")
        self.assertIn("IQ200_MODE_WEBRADIO", available)
        self.assertNotIn("IQ200_MODE_BLUETOOTH", available)
        self.assertNotIn("IQ200_MODE_RADIO", available)
        self.assertIn('ready ? "READY" : "FUTURE"', self.center_ui)

    def test_mode_center_boot_is_minimal(self):
        setup = self.main[self.main.index("void setup()") : self.main.index("void loop()")]
        center = block_after(setup, "if (activeMode == IQ200_MODE_CENTER)")
        self.assertIn("display.begin()", center)
        self.assertIn("modeCenterTask", center)
        self.assertLess(center.index("modeCenterUi.begin()"), center.index("xTaskCreatePinnedToCore"))
        for forbidden in (
            "connectivityManager.begin",
            "webServerService.begin",
            "audioEngine.begin",
            "radioService.begin",
            "storageService",
            "mediaEngine.begin",
        ):
            self.assertNotIn(forbidden, center)

    def test_lightweight_mode_uis_use_stable_framebuffer_present(self):
        for source, prefix in ((self.center_ui, "MODE_UI"), (self.radio_ui, "WEBRADIO_UI")):
            self.assertIn("LGFX_Sprite fb", source)
            self.assertIn("fb.setPsram(true)", source)
            self.assertIn("fb.createSprite(480, 320)", source)
            self.assertIn("display.startWrite()", source)
            self.assertIn("fb.pushSprite(0, 0)", source)
            self.assertIn("display.endWrite()", source)
            self.assertIn(f'[{prefix}] framebuffer=', source)

    def test_webradio_boot_does_not_initialize_local_platform(self):
        setup = self.main[self.main.index("void setup()") : self.main.index("void loop()")]
        radio = block_after(setup, "if (activeMode == IQ200_MODE_WEBRADIO)")
        for required in (
            "connectivityManager.begin",
            "radioService.begin",
            "webServerService.begin",
            "webRadioCoreTask",
            "webRadioUiTask",
        ):
            self.assertIn(required, radio)
        for forbidden in (
            "mediaEngine.begin",
            "mediaCore.begin",
            "iqPlayerCore.begin",
            "resumeEngine.begin",
            "mediaPipeline.begin",
            "databaseService.begin",
            "favoriteManager.begin",
            "scanService.begin",
            "audioEngine.begin",
            "sdMountRequest = true",
        ):
            self.assertNotIn(forbidden, radio)
        self.assertLess(radio.index("webRadioModeUi.begin()"), radio.index("webRadioUiTask"))

    def test_local_boot_never_starts_webradio_decoder_task(self):
        setup = self.main[self.main.index("void setup()") : self.main.index("void loop()")]
        radio_branch = block_after(setup, "if (activeMode == IQ200_MODE_WEBRADIO)")
        local_tail = setup[setup.index(radio_branch) + len(radio_branch) :]
        self.assertIn("iqPlayerCore.begin", local_tail)
        self.assertIn("audioEngine.begin", local_tail)
        self.assertNotIn("radioService.begin", local_tail)

    def test_radio_decoder_allocates_ram_only_in_webradio_begin(self):
        self.assertIn("Audio* radio = nullptr", self.radio_h)
        self.assertNotIn("Audio radio;", self.radio_h)
        begin = block_after(self.radio_cpp, "void RadioService::begin")
        self.assertIn("new (std::nothrow) Audio()", begin)
        self.assertIn("audio_alloc_failed", begin)

    def test_switch_persists_then_stops_then_reboots(self):
        switch = block_after(self.main, "static void processModeSwitchRequest")
        self.assertLess(switch.index("modeManager.setNext(target)"), switch.index("iqPlayerCore.stopAndWait"))
        self.assertLess(switch.index("radioService.stop()"), switch.index("ESP.restart()", switch.index("radioService.stop()")))
        self.assertIn("resumeEngine.save()", switch)

    def test_early_boot_recovery_and_physical_escape_exist(self):
        self.assertIn("MAX_EARLY_BOOT_FAILURES = 3", self.mode)
        self.assertIn("modeManager.markHealthy()", self.main)
        self.assertIn("now - runtimeState.modeStartedMs < 10000U", self.main)
        self.assertIn("now - lastAttemptMs < 5000U", self.main)
        self.assertIn("forceCenterRequested", self.main)
        self.assertIn("millis() - bothPressedSince >= 2000U", self.main)

    def test_rest_api_is_mode_scoped(self):
        self.assertIn('server.on("/api/mode", HTTP_GET', self.web)
        self.assertIn('server.on("/api/mode/switch", HTTP_POST', self.web)
        self.assertGreaterEqual(self.web.count("local_player_mode_required"), 8)
        self.assertGreaterEqual(self.web.count("webradio_mode_required"), 2)
        self.assertIn("mode_reserved_for_future_hardware", self.web)
        self.assertIn("if (rt && rt->systemMode == IQ200_MODE_LOCAL_PLAYER)", self.web)
        self.assertIn("Do not instantiate the Artwork cache/mutex in the WebRadio platform", self.web)


if __name__ == "__main__":
    unittest.main()
