#pragma once
#include <Arduino.h>
#include "RuntimeState.h"

enum IQServiceState : uint8_t {
  SERVICE_STOPPED = 0,
  SERVICE_IDLE,
  SERVICE_RUNNING,
  SERVICE_BUSY,
  SERVICE_ERROR
};

struct ServiceStatus {
  bool audio = false;
  bool storage = false;
  bool wifi = false;
  bool ota = false;
  bool settings = false;
  bool database = false;
  bool scanner = false;
  bool ui = false;
  uint32_t ticks = 0;
  uint32_t heartbeatMs = 0;
  uint32_t watchdogWarnings = 0;
};

class ServiceManager {
  ServiceStatus st;
  RuntimeState* rt = nullptr;
public:
  void begin(RuntimeState* runtime = nullptr) {
    rt = runtime;
    st.audio = true;
    st.storage = true;
    st.wifi = true;
    st.ota = false;
    st.settings = true;
    st.database = true;
    st.scanner = false;
    st.ui = true;
    st.heartbeatMs = millis();
    if (rt) {
      rt->serviceHeartbeatMs = st.heartbeatMs;
      strncpy(rt->bootPhase, "SERVICES", sizeof(rt->bootPhase) - 1);
      rt->bootPhase[sizeof(rt->bootPhase) - 1] = 0;
    }
  }

  void tick() {
    st.ticks++;
    st.heartbeatMs = millis();
    if (rt) {
      rt->serviceTicks = st.ticks;
      rt->serviceHeartbeatMs = st.heartbeatMs;
      rt->serviceWatchdogWarnings = st.watchdogWarnings;
    }
  }

  void setScannerBusy(bool busy) {
    st.scanner = busy;
  }

  void warn() {
    st.watchdogWarnings++;
    if (rt) rt->serviceWatchdogWarnings = st.watchdogWarnings;
  }

  const ServiceStatus& status() const { return st; }
};
