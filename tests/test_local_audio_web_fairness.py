from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]


def method_body(source: str, signature: str) -> str:
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 0
    for pos in range(brace, len(source)):
        if source[pos] == "{":
            depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0:
                return source[brace : pos + 1]
    raise AssertionError(f"unterminated method: {signature}")


class LocalAudioWebFairnessTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.flac = (ROOT / "src/services/FLACPlayerService.cpp").read_text()
        cls.mp3 = (ROOT / "src/services/MP3PlayerService.cpp").read_text()
        cls.wav = (ROOT / "src/services/WavPlayerService.h").read_text()
        cls.web = (ROOT / "src/services/WebServerService.h").read_text()
        cls.main = (ROOT / "src/main.cpp").read_text()

    def test_flac_blocks_one_tick_after_each_pcm_write(self):
        body = method_body(self.flac, "void FLACPlayerService::rtLoop")
        self.assertLess(body.index("writePCMAll"), body.index("vTaskDelay(pdMS_TO_TICKS(1))"))
        self.assertNotIn("if ((rtLoops & 0x07) == 0) taskYIELD()", body)

    def test_other_local_codecs_use_the_same_fairness_contract(self):
        mp3 = method_body(self.mp3, "void MP3PlayerService::rtLoop")
        wav = method_body(self.wav, "void rtLoop()")
        self.assertLess(mp3.rindex("writePCMAll"), mp3.rindex("vTaskDelay(pdMS_TO_TICKS(1))"))
        self.assertLess(wav.index("writePCMAll"), wav.index("vTaskDelay(pdMS_TO_TICKS(1))"))

    def test_core0_worker_only_keeps_task_creation_fallback(self):
        body = method_body(self.main, "void core0Worker")
        self.assertEqual(body.count("webServerService.tick();"), 1)
        self.assertIn("if (!webTaskCreated) webServerService.tick();", body)

    def test_dedicated_web_task_runs_above_blocking_core0_worker(self):
        body = method_body(self.main, "void webServiceTask")
        self.assertIn("webServerService.tick();", body)
        self.assertIn("vTaskDelay(pdMS_TO_TICKS(5));", body)
        self.assertIn('"web_service"', self.main)
        self.assertIn("webTaskCreated = webTaskRc == pdPASS", self.main)

    def test_web_latency_is_visible_in_api_and_diagnostics(self):
        self.assertIn("webTickGapLastMs", self.web)
        self.assertIn("webTickGapMaxMs", self.web)
        self.assertIn("Web gap max/10s", self.web)
        self.assertIn("now - gapWindowStartedMs >= 10000U", self.web)

    def test_alpha7_version_and_short_environment(self):
        ini = (ROOT / "platformio.ini").read_text()
        self.assertIn("[env:iq200-radio]", ini)
        self.assertIn("9.9_ALPHA7_13_WEBRADIO_WEB_PLAYER", ini)


if __name__ == "__main__":
    unittest.main()
