from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]


class DedicatedWebArtworkGuardTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.web = (ROOT / "src/services/WebServerService.h").read_text()
        cls.art = (ROOT / "src/services/ArtworkCache.h").read_text()
        cls.main = (ROOT / "src/main.cpp").read_text()
        cls.runtime = (ROOT / "src/services/RuntimeState.h").read_text()

    def test_artwork_guard_supports_bounded_lock_wait(self):
        self.assertIn("TickType_t timeout=portMAX_DELAY", self.art)
        self.assertIn("o_.lock(timeout)", self.art)

    def test_status_never_blocks_on_artwork_decode(self):
        self.assertIn("ArtworkCache::ReadGuard artGuard(art, 0)", self.web)
        self.assertIn("artworkWebLimited", self.web)
        self.assertIn("WEB_ARTWORK_MAX_BYTES = 256U * 1024U", self.web)

    def test_artwork_http_handler_is_size_and_time_bounded(self):
        self.assertIn("ArtworkCache::ReadGuard guard(art, pdMS_TO_TICKS(5))", self.web)
        self.assertIn("WEB_ARTWORK_SEND_BUDGET_MS = 900U", self.web)
        self.assertIn("artwork_too_large_for_live_web", self.web)
        self.assertIn("millis() - sendStarted", self.web)
        self.assertIn("client.setTimeout(150)", self.web)

    def test_browser_retries_transient_artwork_failure(self):
        self.assertIn("artworkGeneration=-1;$('artwork').removeAttribute('src')", self.web)
        self.assertIn("if(!$('artwork').hasAttribute('src'))artworkGeneration=-1", self.web)

    def test_web_task_health_is_exposed(self):
        for field in ("webTaskRunning", "webTaskLoops", "webTaskStackHighWater"):
            self.assertIn(field, self.runtime)
            self.assertIn(field, self.web)


if __name__ == "__main__":
    unittest.main()
