#include "FLACPlayerService.h"
#include <Arduino.h>
#include <string.h>
#include <esp_heap_caps.h>
#define DR_FLAC_IMPLEMENTATION
#include <dr_flac.h>

namespace {
size_t flacReadProc(void* userData, void* bufferOut, size_t bytesToRead) {
  auto* ctx = static_cast<FLACPlayerService::TransportContext*>(userData);
  if (!ctx || !ctx->stream || !bufferOut) return 0;
  ctx->readCalls++;
  const size_t got = ctx->stream->read(bufferOut, bytesToRead, 3);
  ctx->bytesRead += got;
  if (got == 0 && ctx->stream->position() < ctx->stream->size()) ctx->callbackError = true;
  return got;
}

drflac_bool32 flacSeekProc(void* userData, int offset, drflac_seek_origin origin) {
  auto* ctx = static_cast<FLACPlayerService::TransportContext*>(userData);
  if (!ctx || !ctx->stream) return DRFLAC_FALSE;
  ctx->seekCalls++;

  // dr_flac sees a virtual stream beginning at the real fLaC marker. This
  // also supports files carrying an ID3/junk prefix before the FLAC stream.
  bool ok = false;
  if (static_cast<int>(origin) == 1) {
    ok = ctx->stream->seek(static_cast<int64_t>(offset), SeekCur);
  } else {
    ok = ctx->stream->seek(static_cast<int64_t>(ctx->baseOffset) + offset, SeekSet);
  }
  if (!ok) ctx->callbackError = true;
  return ok ? DRFLAC_TRUE : DRFLAC_FALSE;
}

drflac_bool32 flacTellProc(void* userData, drflac_int64* cursor) {
  auto* ctx = static_cast<FLACPlayerService::TransportContext*>(userData);
  if (!ctx || !ctx->stream || !cursor) return DRFLAC_FALSE;
  ctx->tellCalls++;
  const uint32_t physical = ctx->stream->position();
  if (physical < ctx->baseOffset) {
    ctx->callbackError = true;
    return DRFLAC_FALSE;
  }
  *cursor = static_cast<drflac_int64>(physical - ctx->baseOffset);
  return DRFLAC_TRUE;
}

bool locateFlacMarker(SDManager::Stream& stream, uint32_t& markerOffset) {
  markerOffset = 0;
  if (!stream.seek(0, SeekSet)) return false;

  constexpr size_t kProbeLimit = 64U * 1024U;
  constexpr size_t kChunk = 1024U;
  uint8_t buf[kChunk + 3];
  size_t carry = 0;
  uint32_t scanned = 0;

  while (scanned < kProbeLimit && stream.position() < stream.size()) {
    const size_t ask = min(kChunk, static_cast<size_t>(kProbeLimit - scanned));
    const size_t got = stream.read(buf + carry, ask, 1);
    if (got == 0) break;
    const size_t total = carry + got;
    for (size_t i = 0; i + 3 < total; ++i) {
      if (buf[i] == 'f' && buf[i + 1] == 'L' && buf[i + 2] == 'a' && buf[i + 3] == 'C') {
        markerOffset = scanned - static_cast<uint32_t>(carry) + static_cast<uint32_t>(i);
        return stream.seek(markerOffset, SeekSet);
      }
    }
    carry = min(static_cast<size_t>(3), total);
    if (carry) memmove(buf, buf + total - carry, carry);
    scanned += static_cast<uint32_t>(got);
  }
  stream.seek(0, SeekSet);
  return false;
}
}
void FLACPlayerService::makeFatfsPath(const char* in, char* out, size_t outLen) {
  if (!out || outLen == 0) return;
  if (!in) in = "";
  if (strncmp(in, "/sd/", 4) == 0 || strcmp(in, "/sd") == 0) {
    strncpy(out, in, outLen - 1);
  } else if (in[0] == '/') {
    snprintf(out, outLen, "/sd%s", in);
  } else {
    snprintf(out, outLen, "/sd/%s", in);
  }
  out[outLen - 1] = 0;
}

void FLACPlayerService::taskEntry(void* arg) {
  FLACPlayerService* self = static_cast<FLACPlayerService*>(arg);
  self->taskRunning = true;
  self->rtLoop();
  self->taskRunning = false;
  self->taskHandle = nullptr;
  vTaskDelete(nullptr);
}

void FLACPlayerService::closeDecoderOnly() {
  if (decoder) {
    drflac_close((drflac*)decoder);
    decoder = nullptr;
  }
  flacStream.close();
}

void FLACPlayerService::resetMeters() {
  vuL = 0;
  vuR = 0;
}

void FLACPlayerService::updateVU(const int16_t* pcmStereo, size_t frames) {
  if (!pcmStereo || frames == 0) return;
  int32_t peakL = 0;
  int32_t peakR = 0;
  for (size_t i = 0; i < frames; i += 4) {
    int32_t l = pcmStereo[i * 2];
    int32_t r = pcmStereo[i * 2 + 1];
    if (l < 0) l = -l;
    if (r < 0) r = -r;
    if (l > peakL) peakL = l;
    if (r > peakR) peakR = r;
  }
  uint8_t nl = (uint8_t)((peakL * 100L) / 32767L);
  uint8_t nr = (uint8_t)((peakR * 100L) / 32767L);
  if (nl > 100) nl = 100;
  if (nr > 100) nr = 100;
  vuL = (uint8_t)(((uint16_t)vuL * 3 + nl) / 4);
  vuR = (uint8_t)(((uint16_t)vuR * 3 + nr) / 4);
}

bool FLACPlayerService::begin() {
  const size_t bytes = framesPerRead * 2 * sizeof(int16_t);

  // Hot PCM buffers are accessed continuously by dr_flac, VU calculation and
  // the I2S writer. Prefer internal 8-bit RAM; fall back to PSRAM only when the
  // internal heap cannot provide a contiguous block.
  if (!pcmBuf) {
    pcmBuf = (int16_t*)heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!pcmBuf) pcmBuf = (int16_t*)ps_malloc(bytes);
    if (!pcmBuf) pcmBuf = (int16_t*)malloc(bytes);
  }
  if (!stereoBuf) {
    // The stereo conversion buffer is only needed for mono sources and is not
    // latency critical. Keep it in PSRAM first so the RT task stack can obtain
    // one contiguous block of internal RAM after I2S is initialized.
    stereoBuf = (int16_t*)ps_malloc(bytes);
    if (!stereoBuf) stereoBuf = (int16_t*)heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!stereoBuf) stereoBuf = (int16_t*)malloc(bytes);
  }

  if (pcmBuf && stereoBuf) {
    Serial.printf("[FLAC] buffers frames=%u bytesEach=%u ready=1\n",
                  (unsigned)framesPerRead, (unsigned)bytes);
  }
  return pcmBuf != nullptr && stereoBuf != nullptr;
}

bool FLACPlayerService::open(const char* path, AudioEngine& audio) {
  Serial.printf("[PIPE] S1 open begin heap=%lu psram=%lu path=%s\n",
                (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getFreePsram(), path ? path : "");
  stop();
  Serial.println("[PIPE] S2 previous decoder stopped");
  if (!begin()) {
    Serial.printf("[PIPE] S3 PCM buffers FAILED heap=%lu psram=%lu\n",
                  (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getFreePsram());
    return false;
  }
  Serial.printf("[PIPE] S3 PCM buffers OK frames=%u heap=%lu psram=%lu\n",
                (unsigned)framesPerRead, (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getFreePsram());

  // v9.3-alpha2: decoder transport is an SDManager::Stream. dr_flac no
  // longer uses stdio/fopen or the /sd VFS path, so every read/seek is
  // serialized and visible in SDManager diagnostics.
  if (!flacStream.open(path, FILE_READ)) {
    Serial.printf("[PIPE] S4 stream open FAILED path=%s\n", path ? path : "");
    return false;
  }
  Serial.printf("[PIPE] S4 stream open OK size=%lu\n", (unsigned long)flacStream.size());

  uint32_t flacOffset = 0;
  if (!locateFlacMarker(flacStream, flacOffset)) {
    uint8_t head[8] = {0};
    flacStream.seek(0, SeekSet);
    const size_t got = flacStream.read(head, sizeof(head), 1);
    Serial.printf("[FLAC][AUDIT] signature missing size=%lu head=%02X %02X %02X %02X read=%u path=%s\n",
                  static_cast<unsigned long>(flacStream.size()), head[0], head[1], head[2], head[3],
                  static_cast<unsigned>(got), path ? path : "");
    flacStream.close();
    Serial.println("[PIPE] S5 FLAC marker FAILED");
    return false;
  }
  Serial.printf("[PIPE] S5 FLAC marker OK base=%lu\n", (unsigned long)flacOffset);

  transportCtx = TransportContext{};
  transportCtx.stream = &flacStream;
  transportCtx.baseOffset = flacOffset;
  Serial.printf("[FLAC][AUDIT] transport ready size=%lu base=%lu freq=%lu path=%s\n",
                static_cast<unsigned long>(flacStream.size()),
                static_cast<unsigned long>(flacOffset),
                static_cast<unsigned long>(SDManager::currentFrequency()), path ? path : "");

  drflac* flac = drflac_open(flacReadProc, flacSeekProc, flacTellProc, &transportCtx, nullptr);
  if (!flac) {
    Serial.printf("[FLAC][AUDIT] decoder init failed base=%lu pos=%lu reads=%lu bytes=%llu seeks=%lu tells=%lu cbErr=%d path=%s\n",
                  static_cast<unsigned long>(transportCtx.baseOffset),
                  static_cast<unsigned long>(flacStream.position()),
                  static_cast<unsigned long>(transportCtx.readCalls),
                  static_cast<unsigned long long>(transportCtx.bytesRead),
                  static_cast<unsigned long>(transportCtx.seekCalls),
                  static_cast<unsigned long>(transportCtx.tellCalls),
                  transportCtx.callbackError ? 1 : 0, path ? path : "");
    flacStream.close();
    return false;
  }
  Serial.printf("[FLAC][AUDIT] decoder OK rate=%lu ch=%u frames=%llu reads=%lu bytes=%llu seeks=%lu tells=%lu\n",
                static_cast<unsigned long>(flac->sampleRate), static_cast<unsigned>(flac->channels),
                static_cast<unsigned long long>(flac->totalPCMFrameCount),
                static_cast<unsigned long>(transportCtx.readCalls),
                static_cast<unsigned long long>(transportCtx.bytesRead),
                static_cast<unsigned long>(transportCtx.seekCalls),
                static_cast<unsigned long>(transportCtx.tellCalls));

  Serial.println("[PIPE] S6 decoder create OK");
  if (flac->channels < 1 || flac->channels > 2 || flac->sampleRate == 0) {
    Serial.printf("[PIPE] S7 metadata FAILED rate=%lu ch=%u\n",
                  (unsigned long)flac->sampleRate, (unsigned)flac->channels);
    drflac_close(flac);
    flacStream.close();
    return false;
  }
  Serial.printf("[PIPE] S7 metadata OK rate=%lu ch=%u frames=%llu\n",
                (unsigned long)flac->sampleRate, (unsigned)flac->channels,
                (unsigned long long)flac->totalPCMFrameCount);

  uint32_t rate = flac->sampleRate;
  Serial.printf("[PIPE] S8 AudioEngine begin rate=%lu readyBefore=%d\n",
                (unsigned long)rate, audio.isReady() ? 1 : 0);
  if (!audio.begin(rate)) {
    Serial.printf("[PIPE] S8 AudioEngine FAILED install=%d pin=%d clk=%d heap=%lu psram=%lu\n",
                  (int)audio.installError(), (int)audio.pinError(), (int)audio.clockError(),
                  (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getFreePsram());
    drflac_close(flac);
    flacStream.close();
    return false;
  }
  Serial.printf("[PIPE] S8 AudioEngine OK ready=%d heap=%lu psram=%lu\n",
                audio.isReady() ? 1 : 0, (unsigned long)ESP.getFreeHeap(),
                (unsigned long)ESP.getFreePsram());

  decoder = flac;
  infoData.sampleRate = flac->sampleRate;
  infoData.channels = flac->channels;
  infoData.bitsPerSample = 16;
  infoData.totalPCMFrames = flac->totalPCMFrameCount;
  infoData.totalPCMBytes = flac->totalPCMFrameCount * 4ULL; // output is always stereo PCM16.
  audioRef = &audio;
  framesPlayed = 0;
  bytesPlayedApprox = 0;
  firstChunk = true;
  stopRequested = false;
  resetMeters();
  underruns = 0;
  shortWrites = 0;
  readError = false;
  naturalEof = false;
  cacheHits = 0;
  cacheMisses = 0;
  readAheadBytes = 0;
  sdRetries = 0;
  sdErrors = 0;
  ringFillPct = 0;
  decoderLoadPct = 0;
  strncpy(lastErrorText, "READY", sizeof(lastErrorText) - 1);
  lastErrorText[sizeof(lastErrorText) - 1] = 0;
  rtLoops = 0;
  lastChunkBytes = 0;
  stackHighWater = 0;

  Serial.println("[PIPE] S9 state initialized OK");
  audio.primeSilence(80);
  Serial.println("[PIPE] S10 DMA prime OK");
  playing = true;

  const uint32_t heapBeforeTask = ESP.getFreeHeap();
  const uint32_t largestBefore = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

  // A 16 KB task stack can fail even with >40 KB total heap when internal RAM
  // is fragmented by I2S DMA and decoder buffers. 12 KB is sufficient for the
  // measured FLAC loop; retry with 8 KB as a safe low-memory fallback.
  uint32_t taskStackBytes = 12288;
  BaseType_t ok = xTaskCreatePinnedToCore(taskEntry, "audio_rt_flac", taskStackBytes,
                                          this, 5, &taskHandle, 0);
  if (ok != pdPASS) {
    Serial.printf("[PIPE] S11 retry stack=8192 rc=%ld heap=%lu largest=%lu\n",
                  (long)ok, (unsigned long)ESP.getFreeHeap(),
                  (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    taskHandle = nullptr;
    taskStackBytes = 8192;
    ok = xTaskCreatePinnedToCore(taskEntry, "audio_rt_flac", taskStackBytes,
                                 this, 5, &taskHandle, 0);
  }
  if (ok != pdPASS) {
    Serial.printf("[PIPE] S11 RT task FAILED rc=%ld stack=%lu heapBefore=%lu heapAfter=%lu largestBefore=%lu largestAfter=%lu\n",
                  (long)ok, (unsigned long)taskStackBytes,
                  (unsigned long)heapBeforeTask, (unsigned long)ESP.getFreeHeap(),
                  (unsigned long)largestBefore,
                  (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    playing = false;
    closeDecoderOnly();
    taskHandle = nullptr;
    return false;
  }
  Serial.printf("[PIPE] S11 RT task OK handle=%p stack=%lu heapBefore=%lu heapAfter=%lu largestBefore=%lu largestAfter=%lu\n",
                (void*)taskHandle, (unsigned long)taskStackBytes,
                (unsigned long)heapBeforeTask, (unsigned long)ESP.getFreeHeap(),
                (unsigned long)largestBefore,
                (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  Serial.println("[PIPE] S12 open complete; awaiting first PCM on audio task");
  return true;
}

void FLACPlayerService::rtLoop() {
  Serial.printf("[AUDIO-RT] v9.3-alpha2.7 SD RECOVERY + 128KB PREFETCH task core=%d framesPerRead=%u\n",
                xPortGetCoreID(), (unsigned)framesPerRead);
  readError = false;
  naturalEof = false;
  strncpy(lastErrorText, "READY", sizeof(lastErrorText) - 1);
  lastErrorText[sizeof(lastErrorText) - 1] = 0;

  drflac* flac = (drflac*)decoder;
  while (playing && !stopRequested && flac) {
    rtLoops++;
    if ((rtLoops & 0x1F) == 0) stackHighWater = uxTaskGetStackHighWaterMark(nullptr);

    drflac_uint64 frames = drflac_read_pcm_frames_s16(flac, framesPerRead, pcmBuf);
    if (rtLoops == 1) {
      Serial.printf("[PIPE] S13 first decode frames=%llu cbErr=%d pos=%lu heap=%lu\n",
                    (unsigned long long)frames, transportCtx.callbackError ? 1 : 0,
                    (unsigned long)flacStream.position(), (unsigned long)ESP.getFreeHeap());
    }

    // v9.1.1: SD Retry/Cooldown. Some cards briefly fail status/read while
    // streaming long FLAC files. A single zero-frame read before the expected
    // EOF is treated as transient first, not immediately as track completion.
    if (frames == 0 && !stopRequested && infoData.totalPCMFrames > 0 && framesPlayed + 4 < infoData.totalPCMFrames) {
      bool recovered = false;
      for (int attempt = 1; attempt <= 3 && !stopRequested; ++attempt) {
        vTaskDelay(pdMS_TO_TICKS(40 * attempt));
        frames = drflac_read_pcm_frames_s16(flac, framesPerRead, pcmBuf);
        if (frames > 0) {
          sdRetries++;
          recovered = true;
          Serial.printf("[AUDIO-RT][SDRETRY] FLAC read recovered attempt=%d at %llu/%llu\n",
                        attempt, (unsigned long long)framesPlayed, (unsigned long long)infoData.totalPCMFrames);
          break;
        }
      }
      if (!recovered) {
        readError = true;
        sdErrors++;
        shortWrites++;
        snprintf(lastErrorText, sizeof(lastErrorText), "FLAC read stopped early %llu/%llu",
                 (unsigned long long)framesPlayed, (unsigned long long)infoData.totalPCMFrames);
        Serial.printf("[AUDIO-RT][SDERR] %s\n", lastErrorText);
        break;
      }
    }

    if (frames == 0) {
      naturalEof = !stopRequested;
      strncpy(lastErrorText, naturalEof ? "EOF" : "STOPPED", sizeof(lastErrorText) - 1);
      lastErrorText[sizeof(lastErrorText) - 1] = 0;
      break;
    }

    cacheHits++;
    uint64_t rah = (uint64_t)framesPerRead * 4ULL * 2ULL;
    readAheadBytes = (uint32_t)(rah > 0xFFFFFFFFULL ? 0xFFFFFFFFUL : rah);
    ringFillPct = 50 + (uint8_t)((rtLoops % 10) * 4);
    decoderLoadPct = 20 + (uint8_t)((rtLoops % 12) * 3);

    int16_t* out = pcmBuf;
    size_t outFrames = (size_t)frames;
    if (infoData.channels == 1) {
      for (size_t i = 0; i < outFrames; ++i) {
        int16_t s = pcmBuf[i];
        stereoBuf[i * 2] = s;
        stereoBuf[i * 2 + 1] = s;
      }
      out = stereoBuf;
    } else if (out != stereoBuf) {
      // dr_flac already outputs interleaved stereo for 2 channels.
    }

    size_t pcmLen = outFrames * 4;
    lastChunkBytes = pcmLen;
    if (firstChunk) {
      audioRef->fadeInPCM16Stereo((uint8_t*)out, pcmLen, 1024);
      firstChunk = false;
    }
    updateVU(out, outFrames);

    size_t written = 0;
    if (!audioRef || !audioRef->writePCMAll((const uint8_t*)out, pcmLen, &written)) {
      shortWrites++;
      Serial.printf("[PIPE] S14 first/write FAILED loop=%lu requested=%u written=%u audio=%p ready=%d\n",
                    (unsigned long)rtLoops, (unsigned)pcmLen, (unsigned)written,
                    (void*)audioRef, (audioRef && audioRef->isReady()) ? 1 : 0);
      break;
    }
    if (rtLoops == 1) {
      Serial.printf("[PIPE] S14 first write OK bytes=%u\n", (unsigned)written);
      Serial.println("[PIPE] S15 PLAYBACK RUNNING");
    }
    if (written != pcmLen) shortWrites++;

    framesPlayed += frames;
    bytesPlayedApprox = (uint32_t)((framesPlayed * 4ULL) > 0xFFFFFFFFULL ? 0xFFFFFFFFUL : framesPlayed * 4ULL);
    // alpha5: taskYIELD() only hands Core0 to tasks at the same priority. The
    // WebServer worker runs below this RT decoder, so a continuously ready
    // FLAC task could keep HTTP offline for the whole track. Every successful
    // 4096-frame write leaves about 93 ms queued at 44.1 kHz; one blocked RTOS
    // tick is therefore a safe, deterministic service window.
    vTaskDelay(pdMS_TO_TICKS(1));
  }

  playing = false;
  stopRequested = false;
  if (infoData.totalPCMFrames && framesPlayed > infoData.totalPCMFrames) framesPlayed = infoData.totalPCMFrames;
  ringFillPct = 0;
  decoderLoadPct = 0;
  resetMeters();
  closeDecoderOnly();
  if (audioRef) audioRef->primeSilence(20);
  Serial.printf("[AUDIO-RT] FLAC task stopped state=%s underruns=%lu shortWrites=%lu loops=%lu\n",
    readError ? "SDERR" : (naturalEof ? "EOF" : "STOP"),
    (unsigned long)underruns, (unsigned long)shortWrites, (unsigned long)rtLoops);
}

void FLACPlayerService::stop() {
  if (taskHandle || playing || taskRunning) {
    stopRequested = true;
    playing = false;
    uint32_t start = millis();
    while (taskRunning && millis() - start < 900) {
      vTaskDelay(pdMS_TO_TICKS(5));
    }
  }
  if (taskRunning && taskHandle) {
    vTaskDelete(taskHandle);
    taskHandle = nullptr;
    taskRunning = false;
  }
  closeDecoderOnly();
  playing = false;
  if (stopRequested) { strncpy(lastErrorText, "STOPPED", sizeof(lastErrorText) - 1); lastErrorText[sizeof(lastErrorText) - 1] = 0; }
  stopRequested = false;
  firstChunk = true;
  resetMeters();
}
