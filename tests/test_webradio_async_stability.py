from pathlib import Path
import re
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


class WebRadioAsyncStabilityTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.radio = (ROOT / "src/services/RadioService.cpp").read_text()
        cls.web = (ROOT / "src/services/WebServerService.h").read_text()
        cls.main = (ROOT / "src/main.cpp").read_text()
        cls.resume = (ROOT / "src/services/ResumeEngine.h").read_text()
        cls.radio_ui = (ROOT / "src/ui/WebRadioModeUI.h").read_text()

    def test_rest_play_only_enqueues(self):
        play = method_body(self.radio, "bool RadioService::play")
        self.assertIn("xQueueOverwrite", play)
        self.assertNotIn("connectNow", play)
        self.assertNotIn("connecttohost", play)
        self.assertNotIn("stopAndWait", play)
        self.assertNotIn("stopSong", play)

    def test_audio_library_calls_stay_out_of_webserver(self):
        for forbidden in ("connecttohost", "stopSong", "radio.loop", "localAudio->stop"):
            self.assertNotIn(forbidden, self.web)
        self.assertIn('sendJson(ok ? 202 : 503', self.web)

    def test_radio_status_uses_snapshot_not_mutable_string_refs(self):
        self.assertIn("radioPlayback->snapshot", self.web)
        self.assertNotIn("currentTitle()", self.web)
        self.assertNotIn("radioPlayback->error()", self.web)
        self.assertIn("char title[128]", (ROOT / "src/services/RadioService.h").read_text())

    def test_browser_polling_is_bounded(self):
        self.assertIn("AbortController", self.web)
        self.assertIn("updateInFlight", self.web)
        self.assertIn("radioStatusInFlight", self.web)
        self.assertIn("setInterval(update,750)", self.web)

    def test_unsupported_codec_stops_without_reconnect_loop(self):
        task = method_body(self.radio, "void RadioService::taskLoop")
        info = method_body(self.radio, "void RadioService::onInfo")
        eof = method_body(self.radio, "void RadioService::onEof")
        self.assertIn("fatalStreamErrorPending", info)
        self.assertIn('lower.indexOf("not supported")', info)
        self.assertIn('setState(ERROR_STATE, "unsupported_stream_format")', task)
        self.assertIn("reconnectRequested = false", task)
        self.assertIn("if (fatalStreamErrorPending) return", eof)
        self.assertIn("radioUnsupportedStreams", self.web)

    def test_local_play_waits_for_radio_i2s_release(self):
        self.assertIn("runtimeState.wavPlayRequest && radioService.isActive()", self.main)
        self.assertIn("local play waiting for WebRadio I2S release", self.main)
        self.assertIn("runtimeState.radioTakeoverStop", self.main)

    def test_resume_list_position_fix_is_included(self):
        self.assertIn('line.startsWith("PLAYLIST_INDEX=")', self.resume)
        self.assertIn("_rt->resumeLoadedPlaylistIndex = playlistIndex;", self.resume)
        self.assertIn('restorePlaylistPositionFromResume("BOOT")', self.main)
        self.assertIn('restorePlaylistPositionFromResume("MANUAL_LOAD")', self.main)

    def test_short_build_environment_and_version(self):
        ini = (ROOT / "platformio.ini").read_text()
        self.assertIn("[env:iq200-radio]", ini)
        self.assertIn("9.9_ALPHA7_13_WEBRADIO_WEB_PLAYER", ini)
        self.assertNotRegex(ini, r"\[env:.{40,}\]")

    def test_webradio_vu_is_sampled_only_in_decoder_task(self):
        task = method_body(self.radio, "void RadioService::taskLoop")
        sample = method_body(self.radio, "void RadioService::sampleVu")
        self.assertIn("sampleVu();", task)
        self.assertNotIn("radio->getVUlevel", self.radio)
        self.assertIn("pcmVuLeft", sample)
        self.assertIn("pcmVuRight", sample)
        self.assertIn("void audio_process_i2s(uint32_t* sample, bool* continueI2S)", self.radio)
        self.assertIn("*continueI2S = true", self.radio)
        self.assertIn("packed >> 16", self.radio)
        self.assertIn("now - lastVuSampleMs < 25U", sample)
        self.assertIn("radioVuTicks++", sample)

    def test_webradio_vu_uses_bounded_partial_sprite(self):
        draw = method_body(self.radio_ui, "void drawVu(bool force = false)")
        self.assertIn("LGFX_Sprite vuFb", self.radio_ui)
        self.assertIn("vuFb.createSprite(440, 48)", self.radio_ui)
        self.assertIn("1000U / constrain(rt.radioVuFps, 10, 30)", draw)
        self.assertIn("vuFb.pushSprite(20, 232)", draw)
        self.assertNotIn("fb.pushSprite(0, 0)", draw)
        self.assertIn("radioVuL", self.web)
        self.assertIn("radioVuR", self.web)

    def test_webradio_web_settings_are_mode_safe_and_persistent(self):
        main = self.main
        radio_h = (ROOT / "src/services/RadioService.h").read_text()
        self.assertIn("webRadioModeUi.handleCommand(command)", main)
        self.assertIn("iq200-radio-ui", self.radio_ui)
        for command in ("theme ", "brightness ", "vu fps ", "vu hold ", "vu decay ", "eq custom "):
            self.assertIn(command, self.radio_ui)
        self.assertIn("eqUpdatePending", radio_h)
        self.assertIn("radio->setTone", self.radio)
        self.assertIn("Display & Volume", self.web)
        self.assertIn("appearanceTab.hidden=!(local||wr)", self.web)

    def test_webradio_tft_uses_utf8_cyrillic_font_for_metadata(self):
        font = (ROOT / "src/ui/Font5x7.h").read_text()
        self.assertIn('#include "Font5x7.h"', self.radio_ui)
        self.assertIn("hasUtf8(text)", self.radio_ui)
        self.assertIn("iqDrawTextClipped", self.radio_ui)
        self.assertGreaterEqual(self.radio_ui.count("iqDrawTextClipped"), 2)
        for letter in ("U'І'", "U'Ї'", "U'Є'", "U'Ґ'", "U'А'", "U'я'"):
            self.assertIn(letter, font)

    def test_visuals_are_bounded_and_audio_first(self):
        art = (ROOT / "src/services/RadioArtworkCache.h").read_text()
        store = (ROOT / "src/services/RadioStationStore.h").read_text()
        self.assertIn("MAX_BYTES = 256U * 1024U", art)
        self.assertIn("setConnectTimeout(1200)", art)
        self.assertIn("before connecttohost()", self.radio)
        self.assertLess(self.radio.index("RadioArtworkCache::instance().fetch"), self.radio.index("const bool ok = connectNow()"))
        self.assertIn("String artwork", store)
        self.assertIn("drawMarquee(false)", self.radio_ui)
        self.assertIn("vu gain ", self.radio_ui)
        self.assertIn("vu gate ", self.radio_ui)
        self.assertIn("style == \"neon\"", self.radio_ui)
        self.assertIn("style == \"center\"", self.radio_ui)

    def test_webradio_layers_do_not_trigger_periodic_full_frame_push(self):
        draw = method_body(self.radio_ui, "void draw(bool force = false)")
        tick = method_body(self.radio_ui, "void tick()")
        self.assertNotIn("lastDrawMs < 1000U", draw)
        self.assertNotIn("lastTitle", draw)
        self.assertNotIn("lastVolume", draw)
        self.assertIn("if (!changed) return", draw)
        self.assertIn("drawVolumeInfo()", self.radio_ui)
        self.assertIn("drawFooter()", tick)

    def test_webradio_vu_style_selection_survives_status_polling(self):
        self.assertIn("vuStylePending", self.web)
        self.assertIn("vuStyleEditing", self.web)
        self.assertIn("$('vustyle').onchange", self.web)
        self.assertIn("apiVuStyle===vuStylePending", self.web)

    def test_six_vu_styles_have_distinct_rendering_paths(self):
        draw = method_body(self.radio_ui, "void drawVuCanvas")
        self.assertIn("continuous analogue bar", draw)
        self.assertIn("narrow vertical needles", draw)
        self.assertIn("solid wide rectangular LED", draw)
        self.assertIn("circular LED points", draw)
        self.assertIn("luminous core", draw)
        self.assertIn("mirrored blocks grow", draw)
        self.assertIn("if (rt.radioVuStyle == 0)", draw)
        self.assertIn("else if (rt.radioVuStyle == 4)", draw)

    def test_station_browser_uses_nav_encoder_and_async_radio_queue(self):
        self.assertIn("RadioStationStore& stations", self.radio_ui)
        self.assertIn("void drawStationBrowser()", self.radio_ui)
        self.assertIn("const int navDelta = nav.delta(false, 2)", self.radio_ui)
        self.assertIn("if (stationBrowserOpen) playSelectedStation()", self.radio_ui)
        self.assertIn("radio.play(name, url, artwork)", self.radio_ui)
        self.assertIn("if (stationBrowserOpen) return", self.radio_ui)
        self.assertIn("RadioStationStore radioStationStore", self.main)

    def test_webradio_web_player_has_transport_artwork_vu_and_volume(self):
        for element_id in (
            'radioPlayerArtwork', 'radioPlayerStation', 'radioPlayerTitle',
            'radioPlayerState', 'radioPlayerVuL', 'radioPlayerVuR',
            'radioPrev', 'radioPlay', 'radioStop', 'radioNext', 'radioVol'
        ):
            self.assertIn(f'id="{element_id}"', self.web)
        self.assertIn('function stepRadio(direction)', self.web)
        self.assertIn("run('volume '+e.target.value)", self.web)
        self.assertIn(r'\"vuLeft\"', self.web)
        self.assertIn(r'\"vuRight\"', self.web)
        self.assertIn(r'\"artwork\"', self.web)


if __name__ == "__main__":
    unittest.main()
