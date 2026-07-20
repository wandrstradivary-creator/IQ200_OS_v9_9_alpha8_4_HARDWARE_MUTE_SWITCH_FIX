#include "RadioService.h"
#include <WiFi.h>
#include "RadioArtworkCache.h"

RadioService* RadioService::activeInstance = nullptr;

// ESP32-audioI2S callback hooks. Keep these global with exact names.
void audio_info(const char* info) { if (RadioService::activeInstance) RadioService::activeInstance->onInfo(info); }
void audio_showstation(const char* info) { if (RadioService::activeInstance) RadioService::activeInstance->onStation(info); }
void audio_showstreamtitle(const char* info) { if (RadioService::activeInstance) RadioService::activeInstance->onStreamTitle(info); }
void audio_bitrate(const char* info) { if (RadioService::activeInstance) RadioService::activeInstance->onBitrate(info); }
void audio_eof_stream(const char* info) { if (RadioService::activeInstance) RadioService::activeInstance->onEof(info); }

// Exact esphome/ESP32-audioI2S 2.0.7 hook from Audio.h. Gain() packs
// left in bits 31..16 and right in bits 15..0. The callback is per frame.
void audio_process_i2s(uint32_t* sample, bool* continueI2S) {
  // In 2.0.7 false means the callback consumed the frame. It must be true so
  // Audio::playSample() continues with its normal i2s_write() call.
  if (continueI2S) *continueI2S = true;
  if (!sample || !RadioService::activeInstance) return;
  const uint32_t packed = *sample;
  const int32_t left = (int16_t)(packed >> 16);
  const int32_t right = (int16_t)(packed & 0xFFFFU);
  const uint16_t peakLeft = (uint16_t)min<int32_t>(32767, abs(left));
  const uint16_t peakRight = (uint16_t)min<int32_t>(32767, abs(right));
  RadioService::activeInstance->onPcmLevel((uint8_t)(peakLeft >> 7), (uint8_t)(peakRight >> 7));
}

String RadioService::cleanMetadata(const char* src) {
  String value = src ? String(src) : String();
  value.trim();
  if (value.startsWith("StreamTitle='")) value.remove(0, 13);
  if (value.endsWith("';")) value.remove(value.length() - 2);
  else if (value.endsWith("'")) value.remove(value.length() - 1);
  value.replace("_", " ");
  value.replace("\t", " ");
  while (value.indexOf("  ") >= 0) value.replace("  ", " ");
  value.trim();
  return value;
}

void RadioService::parseStreamInfo(const char* info) {
  if (!info) return;
  String value(info), lower(value); lower.toLowerCase();
  if (lower.indexOf("aac+") >= 0 || lower.indexOf("aacp") >= 0) copyText(codecName, sizeof(codecName), "AAC+");
  else if (lower.indexOf("aac") >= 0) copyText(codecName, sizeof(codecName), "AAC");
  else if (lower.indexOf("flac") >= 0) copyText(codecName, sizeof(codecName), "FLAC");
  else if (lower.indexOf("ogg") >= 0 || lower.indexOf("vorbis") >= 0) copyText(codecName, sizeof(codecName), "OGG");
  else if (lower.indexOf("mp3") >= 0 || lower.indexOf("mpeg") >= 0) copyText(codecName, sizeof(codecName), "MP3");
  int hz = lower.indexOf("hz");
  if (hz > 0) {
    int begin = hz - 1; while (begin >= 0 && isDigit(lower[begin])) --begin;
    const uint32_t parsed = lower.substring(begin + 1, hz).toInt();
    if (parsed >= 8000 && parsed <= 192000) streamSampleRate = parsed;
  }
}

void RadioService::copyText(char* dst, size_t len, const char* src) {
  if (!dst || len == 0) return;
  if (!src) src = "";
  strncpy(dst, src, len - 1);
  dst[len - 1] = 0;
}

const char* RadioService::stateName(State s) {
  switch (s) {
    case IDLE: return "IDLE";
    case QUEUED: return "QUEUED";
    case CONNECTING: return "CONNECTING";
    case BUFFERING: return "BUFFERING";
    case PLAYING: return "PLAYING";
    case RECONNECTING: return "RECONNECTING";
    case ERROR_STATE: return "ERROR";
    case STOPPED: return "STOPPED";
    default: return "UNKNOWN";
  }
}

void RadioService::setState(State next, const char* errorText) {
  state = next;
  if (errorText) copyText(lastError, sizeof(lastError), errorText);
  mirror();
}

void RadioService::mirror() {
  Snapshot next;
  next.state = state;
  next.active = isActive();
  next.sessionStarted = radioSessionStarted;
  copyText(next.station, sizeof(next.station), stationName);
  copyText(next.url, sizeof(next.url), streamUrl);
  copyText(next.title, sizeof(next.title), streamTitle);
  copyText(next.artwork, sizeof(next.artwork), artworkUrl);
  copyText(next.error, sizeof(next.error), lastError);
  copyText(next.codec, sizeof(next.codec), codecName);
  copyText(next.bitrate, sizeof(next.bitrate), bitrateText);
  next.sampleRate = streamSampleRate;
  next.uptimeSeconds = startedMs ? (millis() - startedMs) / 1000U : 0;
  next.commandsQueued = commandsQueued;
  next.commandsDropped = commandsDropped;
  next.connectAttempts = connectAttempts;
  next.reconnects = reconnects;
  next.unsupportedStreams = unsupportedStreams;
  next.taskStackHighWater = taskHandle ? uxTaskGetStackHighWaterMark(taskHandle) : 0;

  portENTER_CRITICAL(&snapshotMux);
  cached = next;
  portEXIT_CRITICAL(&snapshotMux);

  if (!rt) return;
  rt->radioPlaying = next.active;
  copyText(rt->radioStatus, sizeof(rt->radioStatus), stateName(next.state));
  copyText(rt->radioStation, sizeof(rt->radioStation), next.station);
  copyText(rt->radioTitle, sizeof(rt->radioTitle), next.title);
  copyText(rt->radioError, sizeof(rt->radioError), next.error);
  copyText(rt->radioUrl, sizeof(rt->radioUrl), next.url);
  rt->radioCommandsQueued = next.commandsQueued;
  rt->radioCommandsDropped = next.commandsDropped;
  rt->radioConnectAttempts = next.connectAttempts;
  rt->radioReconnects = next.reconnects;
  rt->radioUnsupportedStreams = next.unsupportedStreams;
  rt->radioTaskStackHighWater = next.taskStackHighWater;
}

void RadioService::snapshot(Snapshot& out) const {
  portENTER_CRITICAL(&snapshotMux);
  out = cached;
  portEXIT_CRITICAL(&snapshotMux);
}

void RadioService::setHardwareMute(bool muted) {
  unmutePending = false;
  if (hardwareMuted == muted) return;
  hardwareMuted = muted;
  digitalWrite(MUTE_PIN, muted ? MUTE_VAL : !MUTE_VAL);
  Serial.printf("[WEBRADIO][MUTE] %s pin=%d level=%d\n",
                muted ? "ON" : "OFF", MUTE_PIN,
                muted ? (int)MUTE_VAL : (int)!MUTE_VAL);
}

void RadioService::scheduleHardwareUnmute(uint32_t delayMs) {
  // Keep the output silent until the decoder has produced stable PCM for a
  // short guard period. This removes switching clicks and compressed-data
  // noise without blocking the real-time radio task.
  unmutePending = true;
  unmuteAtMs = millis() + delayMs;
}

void RadioService::serviceHardwareMute() {
  if (!unmutePending) return;
  if ((int32_t)(millis() - unmuteAtMs) < 0) return;
  unmutePending = false;
  if (running && radio && radio->isRunning() && state == PLAYING) {
    setHardwareMute(false);
  }
}

void RadioService::begin(RuntimeState& stateRef, IQPlayerCore& playerRef, AudioEngine& audioRef) {
  rt = &stateRef;
  localPlayer = &playerRef;
  localAudio = &audioRef;
  pinMode(MUTE_PIN, OUTPUT);
  hardwareMuted = false;  // force first write through setHardwareMute()
  setHardwareMute(true);  // silent during boot and decoder initialization
  if (!radio) radio = new (std::nothrow) Audio();
  if (!radio) {
    setState(ERROR_STATE, "audio_alloc_failed");
    Serial.println("[WEBRADIO][ERROR] ESP32-audioI2S allocation failed");
    return;
  }
  if (!commandQueue) commandQueue = xQueueCreate(1, sizeof(Command));
  activeInstance = this;
  radio->setPinout(IQ200_I2S_BCK, IQ200_I2S_LRCK, IQ200_I2S_DOUT);
  radio->setVolume(2); // safe boot, updated by webradio_rt before play
  Preferences eqPrefs;
  if (eqPrefs.begin("iq200-radio", true)) {
    requestedEqEnabled = eqPrefs.getBool("eqOn", true);
    requestedEqBass = constrain(eqPrefs.getChar("eqBass", 0), -12, 12);
    requestedEqMid = constrain(eqPrefs.getChar("eqMid", 0), -12, 12);
    requestedEqTreble = constrain(eqPrefs.getChar("eqTreble", 0), -12, 12);
    const String preset = eqPrefs.getString("eqPreset", "flat");
    copyText(rt->eqPreset, sizeof(rt->eqPreset), preset.c_str());
    eqPrefs.end();
  }
  rt->eqEnabled = requestedEqEnabled;
  rt->eqBassDb = requestedEqBass;
  rt->eqMidDb = requestedEqMid;
  rt->eqTrebleDb = requestedEqTreble;
  eqUpdatePending = true;
  setState(IDLE);
  if (!taskHandle) {
    // Priority 2 plus a bounded delay keeps the stream serviced while leaving
    // regular Core0 windows for WebServer and system services.
    xTaskCreatePinnedToCore(taskEntry, "webradio_rt", 8192, this, 2, &taskHandle, 0);
  }
}

void RadioService::taskEntry(void* arg) {
  static_cast<RadioService*>(arg)->taskLoop();
  vTaskDelete(nullptr);
}

void RadioService::taskLoop() {
  for (;;) {
    Command command;
    if (commandQueue && xQueueReceive(commandQueue, &command, 0) == pdTRUE) {
      handleCommand(command);
    }

    applyVolume();
    applyEqualizer();
    serviceHardwareMute();

    if (running && radio) {
      radio->loop();
      sampleVu();
      if (fatalStreamErrorPending) {
        // Library callbacks execute inside radio.loop(). Defer stopSong until
        // the callback has returned, then suppress the EOF callback's normal
        // reconnect path. Unsupported codecs must settle in ERROR, not loop.
        if (radioSessionStarted) radio->stopSong();
        radioSessionStarted = false;
        running = false;
        reconnectRequested = false;
        reconnectAttempt = 0;
        fatalStreamErrorPending = false;
        setHardwareMute(true);
        resetVu();
        setState(ERROR_STATE, "unsupported_stream_format");
        Serial.println("[WEBRADIO] unsupported stream stopped; reconnect disabled");
      } else if (radio->isRunning()) {
        lastDataMs = millis();
        if (state == CONNECTING || state == BUFFERING || state == RECONNECTING) {
          setState(PLAYING);
          scheduleHardwareUnmute(120);
        }
      } else if ((state == PLAYING || state == BUFFERING) && millis() - lastDataMs > 3000) {
        setHardwareMute(true);
        reconnectRequested = true;
        reconnectAtMs = millis() + min<uint32_t>(10000, 1000U << min<uint8_t>(reconnectAttempt, 3));
        setState(RECONNECTING, "stream_stalled");
      }
    }

    if (reconnectRequested && (int32_t)(millis() - reconnectAtMs) >= 0) {
      reconnectRequested = false;
      reconnectAttempt++;
      reconnects++;
      connectNow();
    }

    if (millis() - lastMirrorMs >= 100) {
      lastMirrorMs = millis();
      mirror();
    }
    vTaskDelay(pdMS_TO_TICKS(running ? 2 : 10));
  }
}

void RadioService::sampleVu() {
  const uint32_t now = millis();
  if (now - lastVuSampleMs < 25U) return;  // 40 Hz: enough motion, negligible RT cost
  lastVuSampleMs = now;

  uint8_t rawLeft = 0;
  uint8_t rawRight = 0;
  if (radio && radio->isRunning() && state == PLAYING) {
    rawLeft = pcmVuLeft;
    rawRight = pcmVuRight;
    pcmVuLeft = 0;
    pcmVuRight = 0;
  }
  const uint8_t gate = rt ? rt->radioVuGate : 2;
  const uint16_t userGain = rt ? constrain(rt->radioVuGain, 50, 300) : 100;
  rawLeft = rawLeft <= gate ? 0 : rawLeft - gate;
  rawRight = rawRight <= gate ? 0 : rawRight - gate;
  const uint16_t instant = max<uint16_t>(rawLeft, rawRight);
  if (instant > vuAgcPeak) vuAgcPeak = instant;
  else if (vuAgcPeak > 48) vuAgcPeak--; // slow AGC release, no pumping
  const uint16_t agcBase = (vuAgcPeak < 48U) ? 48U : vuAgcPeak;
  const uint16_t agcCalculated = (uint16_t)((190U * 100U) / agcBase);
  const uint16_t agcGain = (agcCalculated < 70U) ? 70U :
                           ((agcCalculated > 350U) ? 350U : agcCalculated);
  const uint32_t totalGain = (uint32_t)userGain * agcGain;
  rawLeft = (uint8_t)min<uint32_t>(255, ((uint32_t)rawLeft * totalGain) / 10000U);
  rawRight = (uint8_t)min<uint32_t>(255, ((uint32_t)rawRight * totalGain) / 10000U);

  // Fast attack and bounded release avoid flicker without allocating or locking.
  const uint8_t release = rt ? constrain(rt->radioVuDecay, 1, 10) : 5;
  vuLeft = rawLeft >= vuLeft ? rawLeft : (vuLeft > release ? vuLeft - release : 0);
  vuRight = rawRight >= vuRight ? rawRight : (vuRight > release ? vuRight - release : 0);
  if (rt) {
    rt->radioVuLeft = vuLeft;
    rt->radioVuRight = vuRight;
    rt->radioVuTicks++;
  }
}

void RadioService::resetVu() {
  vuLeft = 0;
  vuRight = 0;
  pcmVuLeft = 0;
  pcmVuRight = 0;
  lastVuSampleMs = 0;
  if (rt) {
    rt->radioVuLeft = 0;
    rt->radioVuRight = 0;
  }
}

void RadioService::handleCommand(const Command& command) {
  if (command.type == CMD_STOP) {
    stopCommandPending = false;
    handleStop();
  }
  else if (command.type == CMD_PLAY) handlePlay(command);
}

void RadioService::handleStop() {
  setHardwareMute(true);
  fatalStreamErrorPending = false;
  reconnectRequested = false;
  reconnectAttempt = 0;
  if (radioSessionStarted && radio) {
    radio->stopSong();
    radioSessionStarted = false;
  }
  running = false;
  resetVu();
  if (rt) rt->radioTakeoverStop = false;
  setState(STOPPED);
}

void RadioService::handlePlay(const Command& command) {
  // Assert hardware mute before stopping the previous station/I2S session.
  setHardwareMute(true);
  fatalStreamErrorPending = false;
  resetVu();
  setState(QUEUED);
  // WebRadio owns I2S while active. Stop the local decoder first, but do this
  // only in the radio task so the REST handler can return immediately.
  if (localPlayer && (localPlayer->isPlaying() || localPlayer->isTaskRunning())) {
    if (rt) {
      rt->radioTakeoverStop = true;
      rt->navManualControlPending = true;
    }
    localPlayer->stopAndWait(1800);
  }
  if (localAudio) localAudio->stop();

  // If Stop arrived while the local player was shutting down, honor it before
  // any DNS/HTTP connection is opened.
  Command pending;
  if (commandQueue && xQueuePeek(commandQueue, &pending, 0) == pdTRUE && pending.type == CMD_STOP) {
    xQueueReceive(commandQueue, &pending, 0);
    stopCommandPending = false;
    handleStop();
    return;
  }

  codecName[0] = 0; bitrateText[0] = 0; streamSampleRate = 0;
  copyText(stationName, sizeof(stationName), command.station[0] ? command.station : "Web Radio");
  copyText(streamUrl, sizeof(streamUrl), command.url);
  copyText(artworkUrl, sizeof(artworkUrl), command.artwork);
  streamTitle[0] = 0;
  lastError[0] = 0;
  reconnectAttempt = 0;
  reconnectRequested = false;
  startedMs = millis();

  // Cover transfer is deliberately completed before connecttohost(). It may
  // delay station start briefly, but it never shares WiFi bandwidth with audio.
  if (radioSessionStarted && radio) {
    radio->stopSong();
    radioSessionStarted = false;
    running = false;
  }
  RadioArtworkCache::instance().fetch(artworkUrl);

  const bool ok = connectNow();
  if (ok) {
    Preferences p;
    if (p.begin("iq200-radio", false)) {
      p.putString("lastName", stationName);
      p.putString("lastUrl", streamUrl);
      p.end();
    }
  }
}

void RadioService::applyVolume() {
  if (!radio) return;
  const uint8_t percent = constrain(requestedVolumePercent, 0, 100);
  if (percent == appliedVolumePercent) return;
  appliedVolumePercent = percent;
  const uint8_t libVolume = (uint8_t)map(percent, 0, 100, 0, 21);
  radio->setVolume(libVolume);
}

void RadioService::applyEqualizer() {
  if (!radio || !eqUpdatePending) return;
  const int8_t bass = requestedEqEnabled ? requestedEqBass : 0;
  const int8_t mid = requestedEqEnabled ? requestedEqMid : 0;
  const int8_t treble = requestedEqEnabled ? requestedEqTreble : 0;
  if (bass != appliedEqBass || mid != appliedEqMid || treble != appliedEqTreble) {
    radio->setTone(bass, mid, treble);
    appliedEqBass = bass;
    appliedEqMid = mid;
    appliedEqTreble = treble;
  }
  eqUpdatePending = false;
  Serial.printf("[WEBRADIO][EQ] enabled=%d bass=%d mid=%d treble=%d\n",
                requestedEqEnabled ? 1 : 0, bass, mid, treble);
}

bool RadioService::setEqualizerCustom(int bass, int mid, int treble) {
  if (bass < -12 || bass > 12 || mid < -12 || mid > 12 || treble < -12 || treble > 12) return false;
  requestedEqEnabled = true;
  requestedEqBass = bass;
  requestedEqMid = mid;
  requestedEqTreble = treble;
  if (rt) {
    rt->eqEnabled = true;
    rt->eqBassDb = bass;
    rt->eqMidDb = mid;
    rt->eqTrebleDb = treble;
    copyText(rt->eqPreset, sizeof(rt->eqPreset), "custom");
  }
  Preferences p;
  if (p.begin("iq200-radio", false)) {
    p.putBool("eqOn", true); p.putChar("eqBass", bass); p.putChar("eqMid", mid); p.putChar("eqTreble", treble);
    p.putString("eqPreset", "custom"); p.end();
  }
  eqUpdatePending = true;
  return true;
}

bool RadioService::setEqualizerPreset(const String& requested) {
  String preset = requested; preset.trim(); preset.toLowerCase();
  bool enabled = true; int bass = 0, mid = 0, treble = 0;
  if (preset == "off") enabled = false;
  else if (preset == "flat") {}
  else if (preset == "rock") { bass=5; mid=-2; treble=4; }
  else if (preset == "pop") { bass=2; mid=3; treble=2; }
  else if (preset == "jazz") { bass=3; mid=1; treble=4; }
  else if (preset == "classic" || preset == "classical") { preset="classic"; bass=2; treble=3; }
  else if (preset == "bass") { bass=7; treble=-1; }
  else if (preset == "treble") { bass=-1; treble=7; }
  else if (preset == "vocal") { bass=-2; mid=6; treble=2; }
  else return false;
  requestedEqEnabled = enabled;
  requestedEqBass = bass; requestedEqMid = mid; requestedEqTreble = treble;
  if (rt) {
    rt->eqEnabled = enabled; rt->eqBassDb = bass; rt->eqMidDb = mid; rt->eqTrebleDb = treble;
    copyText(rt->eqPreset, sizeof(rt->eqPreset), preset.c_str());
  }
  Preferences p;
  if (p.begin("iq200-radio", false)) {
    p.putBool("eqOn", enabled); p.putChar("eqBass", bass); p.putChar("eqMid", mid); p.putChar("eqTreble", treble);
    p.putString("eqPreset", preset); p.end();
  }
  eqUpdatePending = true;
  return true;
}

void RadioService::printEqualizer() const {
  Serial.printf("[WEBRADIO][EQ] preset=%s enabled=%d bass=%d mid=%d treble=%d\n",
                rt ? rt->eqPreset : "flat", requestedEqEnabled ? 1 : 0,
                requestedEqBass, requestedEqMid, requestedEqTreble);
}

bool RadioService::connectNow() {
  // DNS, TLS, ICY buffering and decoder resynchronization are always silent.
  setHardwareMute(true);
  connectAttempts++;
  fatalStreamErrorPending = false;
  resetVu();

  // A transient DNS/TLS/ICY failure must not permanently stop WebRadio.
  // Keep retrying with a bounded backoff while a valid station is selected.
  auto scheduleRetry = [this](const char* reason, uint32_t minimumDelayMs) {
    running = false;
    radioSessionStarted = false;
    const uint8_t step = min<uint8_t>(reconnectAttempt, 4U);
    const uint32_t backoff = max<uint32_t>(minimumDelayMs, 1000U << step);
    reconnectRequested = true;
    reconnectAtMs = millis() + min<uint32_t>(15000U, backoff);
    setState(RECONNECTING, reason);
    Serial.printf("[WEBRADIO] retry in %lu ms reason=%s attempt=%u\n",
                  (unsigned long)(reconnectAtMs - millis()), reason, reconnectAttempt);
  };

  if (!radio) {
    reconnectRequested = false;
    running = false;
    setState(ERROR_STATE, "audio_unavailable");
    return false;
  }
  if (strncmp(streamUrl, "http://", 7) != 0 && strncmp(streamUrl, "https://", 8) != 0) {
    reconnectRequested = false;
    running = false;
    setState(ERROR_STATE, "invalid_url");
    return false;
  }
  if (WiFi.status() != WL_CONNECTED) {
    scheduleRetry("wifi_not_connected", 3000U);
    return false;
  }

  setState(reconnectAttempt ? RECONNECTING : CONNECTING);
  if (radioSessionStarted) {
    radio->stopSong();
    radioSessionStarted = false;
  }
  applyVolume();
  const bool ok = radio->connecttohost(streamUrl);
  if (!ok) {
    scheduleRetry("connect_failed", 1500U);
    return false;
  }

  radioSessionStarted = true;
  running = true;
  reconnectRequested = false;
  reconnectAttempt = 0;
  lastDataMs = millis();
  setState(BUFFERING);
  return true;
}

bool RadioService::play(const String& name, const String& url, const String& artwork) {
  if (!commandQueue) return false;
  String cleanName = name;
  String cleanUrl = url;
  String cleanArtwork = artwork;
  cleanName.trim();
  cleanUrl.trim();
  cleanArtwork.trim();
  if (!cleanName.length()) cleanName = "Web Radio";
  if ((!cleanUrl.startsWith("http://") && !cleanUrl.startsWith("https://")) ||
      cleanName.length() > 80 || cleanUrl.length() > 240 || cleanArtwork.length() > 240 ||
      (cleanArtwork.length() && !cleanArtwork.startsWith("http://") && !cleanArtwork.startsWith("https://"))) {
    commandsDropped++;
    return false;
  }

  Command command;
  command.type = CMD_PLAY;
  copyText(command.station, sizeof(command.station), cleanName.c_str());
  copyText(command.url, sizeof(command.url), cleanUrl.c_str());
  copyText(command.artwork, sizeof(command.artwork), cleanArtwork.c_str());
  if (xQueueOverwrite(commandQueue, &command) != pdTRUE) {
    commandsDropped++;
    return false;
  }
  stopCommandPending = false; // latest command wins in the single-slot mailbox
  commandsQueued++;
  state = QUEUED;
  return true;
}

bool RadioService::stop() {
  if (!commandQueue) return false;
  if (stopCommandPending) return true;
  if (!isActive() && (state == IDLE || state == STOPPED || state == ERROR_STATE)) return true;
  Command command;
  command.type = CMD_STOP;
  if (xQueueOverwrite(commandQueue, &command) != pdTRUE) {
    commandsDropped++;
    return false;
  }
  stopCommandPending = true;
  commandsQueued++;
  return true;
}

void RadioService::tick() {
  if (rt) requestedVolumePercent = constrain(rt->volumePercent, 0, 100);
}

void RadioService::print() const {
  Snapshot s;
  snapshot(s);
  Serial.printf("[WEBRADIO] state=%s active=%d session=%d station=%s title=%s url=%s error=%s uptime=%lus queued=%lu dropped=%lu connect=%lu reconnect=%lu unsupported=%lu stackHW=%lu\n",
                stateName(s.state), s.active ? 1 : 0, s.sessionStarted ? 1 : 0,
                s.station, s.title, s.url, s.error,
                (unsigned long)s.uptimeSeconds,
                (unsigned long)s.commandsQueued, (unsigned long)s.commandsDropped,
                (unsigned long)s.connectAttempts, (unsigned long)s.reconnects,
                (unsigned long)s.unsupportedStreams,
                (unsigned long)s.taskStackHighWater);
}

void RadioService::onInfo(const char* info) {
  if (!info) return;
  String message(info);
  String lower = message;
  lower.toLowerCase();
  const bool unsupported = lower.indexOf("not supported") >= 0 ||
                           lower.indexOf("unsupported") >= 0;
  if (unsupported) {
    copyText(lastError, sizeof(lastError), "unsupported_stream_format");
    if (!fatalStreamErrorPending) unsupportedStreams++;
    reconnectRequested = false;
    fatalStreamErrorPending = true;
  } else if (lower.indexOf("failed") >= 0 || lower.indexOf("error") >= 0) {
    copyText(lastError, sizeof(lastError), info);
  }
  parseStreamInfo(info);
  if (lower.indexOf("stream ready") >= 0 || lower.indexOf("audio file") >= 0) {
    setState(PLAYING);
    scheduleHardwareUnmute(120);
  }
  Serial.printf("[WEBRADIO][INFO] %s\n", info);
}

void RadioService::onStation(const char* info) {
  if (info && *info) copyText(stationName, sizeof(stationName), info);
  mirror();
}

void RadioService::onStreamTitle(const char* info) {
  const String cleaned = cleanMetadata(info);
  copyText(streamTitle, sizeof(streamTitle), cleaned.c_str());
  mirror();
}

void RadioService::onBitrate(const char* info) {
  String value = cleanMetadata(info);
  if (value.length()) {
    bool hasUnit = value.indexOf("kbps") >= 0 || value.indexOf("Kbps") >= 0;
    if (!hasUnit) value += " kbps";
    copyText(bitrateText, sizeof(bitrateText), value.c_str());
  }
  parseStreamInfo(info);
  mirror();
  Serial.printf("[WEBRADIO][BITRATE] %s\n", info ? info : "");
}

void RadioService::onEof(const char* info) {
  if (fatalStreamErrorPending) return;
  setHardwareMute(true);
  copyText(lastError, sizeof(lastError), info ? info : "eof");
  radioSessionStarted = false;
  running = false;
  reconnectRequested = true;
  const uint8_t step = min<uint8_t>(reconnectAttempt, 4U);
  reconnectAtMs = millis() + min<uint32_t>(15000U, 1000U << step);
  setState(RECONNECTING, "eof");
  Serial.printf("[WEBRADIO] EOF; reconnect scheduled in %lu ms\n",
                (unsigned long)(reconnectAtMs - millis()));
}

void RadioService::onPcmLevel(uint8_t left, uint8_t right) {
  if (left > pcmVuLeft) pcmVuLeft = left;
  if (right > pcmVuRight) pcmVuRight = right;
}
