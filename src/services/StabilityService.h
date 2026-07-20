#pragma once
#include <Arduino.h>
#include <SD.h>
#include "SDManager.h"
#include "RuntimeState.h"

// v9.1.1 Next Track Engine: lightweight burn-in / recovery supervisor.
// It never blocks UI or audio; it only samples counters and performs small SD probes.
class StabilityService {
  RuntimeState* rt = nullptr;
  uint32_t startMs = 0;
  uint32_t lastTickMs = 0;
  uint32_t lastSdProbeMs = 0;
  uint32_t lastHeapSampleMs = 0;
  uint32_t minHeap = 0xFFFFFFFF;
  uint32_t minPsram = 0xFFFFFFFF;
  uint32_t baselineHeap = 0;
  bool leakWarnLatched = false;
  bool burnin = false;

  void probeSdLatency() {
    if (!rt || !rt->sdOk || !burnin) return;
    // v9.3-alpha2: never inject metadata/status/write traffic while the RT
    // decoder owns the SD transport. The deferred probe runs after playback.
    if (rt->rtAudioTaskRunning || rt->audioPlaying || rt->audioBusy || SDManager::streamActive()) return;
    uint32_t t0 = millis();
    File f = SDManager::open("/iq200/stability.probe", FILE_WRITE);
    if (f) {
      f.print("ok ");
      f.println(millis());
      f.close();
      rt->stabilitySdErrors = 0;
    } else {
      rt->stabilitySdErrors++;
      rt->stabilityRecoveries++;
    }
    uint32_t dt = millis() - t0;
    rt->sdLatencyLastMs = dt;
    if (dt > rt->sdLatencyMaxMs) rt->sdLatencyMaxMs = dt;
    if (dt > rt->stabilitySdLatencyMaxMs) rt->stabilitySdLatencyMaxMs = dt;
  }

public:
  void begin(RuntimeState& state) {
    rt = &state;
    startMs = millis();
    lastTickMs = startMs;
    minHeap = ESP.getFreeHeap();
    minPsram = ESP.getFreePsram();
    baselineHeap = minHeap;
    leakWarnLatched = false;
    rt->stabilityEnabled = true;
    rt->stabilityStartedMs = startMs;
    strncpy(rt->stabilityStatus, "READY", sizeof(rt->stabilityStatus)-1);
    rt->stabilityStatus[sizeof(rt->stabilityStatus)-1] = 0;
  }

  void tick() {
    if (!rt || !rt->stabilityEnabled) return;
    uint32_t now = millis();
    rt->stabilityUptimeMs = now - startMs;
    if (now - lastTickMs < 250) return;
    lastTickMs = now;
    rt->stabilityTicks++;

    if (now - lastHeapSampleMs > 1000) {
      lastHeapSampleMs = now;
      uint32_t h = ESP.getFreeHeap();
      uint32_t p = ESP.getFreePsram();
      if (h < minHeap) minHeap = h;
      if (p < minPsram) minPsram = p;
      rt->stabilityMinHeap = minHeap;
      rt->stabilityMinPsram = minPsram;
      // v9.1.1 RC: avoid false leak warnings during normal WiFi/SD/library startup churn.
      // Latch one warning only after a sustained >32 KB drop from the boot baseline.
      if (!leakWarnLatched && baselineHeap && h + 32768 < baselineHeap && now - startMs > 30000) {
        rt->stabilityLeakWarnings++;
        leakWarnLatched = true;
      }
    }

    if (rt->core0AgeMs > 3000 || rt->core1AgeMs > 3000) {
      rt->stabilityWatchdogWarnings++;
      rt->serviceWatchdogWarnings++;
    }

    if (now - lastSdProbeMs > 10000) {
      lastSdProbeMs = now;
      probeSdLatency();
    }

    if (burnin) {
      strncpy(rt->stabilityStatus, "BURNIN", sizeof(rt->stabilityStatus)-1);
    } else {
      strncpy(rt->stabilityStatus, "READY", sizeof(rt->stabilityStatus)-1);
    }
    rt->stabilityStatus[sizeof(rt->stabilityStatus)-1] = 0;
  }

  void setBurnin(bool on) {
    burnin = on;
    if (rt) {
      rt->burninActive = on;
      rt->burninStartedMs = on ? millis() : 0;
      strncpy(rt->stabilityStatus, on ? "BURNIN" : "READY", sizeof(rt->stabilityStatus)-1);
      rt->stabilityStatus[sizeof(rt->stabilityStatus)-1] = 0;
    }
  }

  void print() const {
    if (!rt) return;
    Serial.println("[STAB] ===== IQ200 v9.1.1 Next Track Engine =====");
    Serial.printf("[STAB] status=%s burnin=%d uptime=%lus ticks=%lu recoveries=%lu\n",
      rt->stabilityStatus, rt->burninActive ? 1 : 0,
      (unsigned long)(rt->stabilityUptimeMs / 1000UL),
      (unsigned long)rt->stabilityTicks,
      (unsigned long)rt->stabilityRecoveries);
    Serial.printf("[STAB] heap min=%lu now=%lu psram min=%lu now=%lu leakWarn=%lu\n",
      (unsigned long)rt->stabilityMinHeap, (unsigned long)ESP.getFreeHeap(),
      (unsigned long)rt->stabilityMinPsram, (unsigned long)ESP.getFreePsram(),
      (unsigned long)rt->stabilityLeakWarnings);
    Serial.printf("[STAB] sd errors=%lu latency last/max=%lu/%lu ms watchdogWarn=%lu eventDrops=%lu busDrops=%lu\n",
      (unsigned long)rt->stabilitySdErrors,
      (unsigned long)rt->sdLatencyLastMs,
      (unsigned long)rt->stabilitySdLatencyMaxMs,
      (unsigned long)rt->stabilityWatchdogWarnings,
      (unsigned long)rt->eventQueueDrops,
      (unsigned long)rt->eventBusDrops);
  }
};
