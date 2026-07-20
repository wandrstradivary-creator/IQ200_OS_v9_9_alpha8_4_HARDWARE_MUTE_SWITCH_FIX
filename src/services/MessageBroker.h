#pragma once
#include <Arduino.h>
#include "EventQueue.h"

enum BrokerCommand : uint8_t {
  CMD_NONE = 0,
  CMD_WIFI_SCAN,
  CMD_SD_MOUNT,
  CMD_AUDIO_TONE,
  CMD_OPEN_APP,
  CMD_NOTIFY
};

struct BrokerMessage {
  BrokerCommand cmd = CMD_NONE;
  int value = 0;
  char text[64] = {0};
};

class MessageBroker {
  QueueHandle_t* uiQueue = nullptr;
  portMUX_TYPE pendingMux = portMUX_INITIALIZER_UNLOCKED;
  volatile uint32_t pendingMask = 0;
  volatile uint32_t coalescedCount = 0;

  static bool isCoalescible(IQEventType type) {
    return type == EVT_WAV_PROGRESS || type == EVT_MONITOR;
  }

  static uint32_t eventBit(IQEventType type) {
    return 1UL << (uint8_t)type;
  }

public:
  void begin(QueueHandle_t* queueRef) {
    uiQueue = queueRef;
  }

  bool postEvent(IQEventType type, int value, uint64_t value64, const char* msg) {
    if (!uiQueue || !(*uiQueue)) return false;

    // v9.3-alpha2.4: high-rate state events are level-triggered, not edge-triggered.
    // Keep at most one pending progress/monitor event in the UI queue. The UI
    // reads the newest RuntimeState values, so duplicate queue entries only add
    // latency and can block important START/STOP/ERROR events.
    const bool coalescible = isCoalescible(type);
    const uint32_t bit = eventBit(type);
    if (coalescible) {
      bool alreadyPending;
      portENTER_CRITICAL(&pendingMux);
      alreadyPending = (pendingMask & bit) != 0;
      if (!alreadyPending) pendingMask |= bit;
      else coalescedCount++;
      portEXIT_CRITICAL(&pendingMux);
      if (alreadyPending) return true;
    }

    IQEvent ev;
    ev.type = type;
    ev.value = value;
    ev.value64 = value64;
    if (msg) {
      strncpy(ev.message, msg, sizeof(ev.message) - 1);
      ev.message[sizeof(ev.message) - 1] = 0;
    }

    const bool ok = xQueueSend(*uiQueue, &ev, 0) == pdTRUE;
    if (!ok && coalescible) {
      portENTER_CRITICAL(&pendingMux);
      pendingMask &= ~bit;
      portEXIT_CRITICAL(&pendingMux);
    }
    return ok;
  }

  void eventConsumed(IQEventType type) {
    if (!isCoalescible(type)) return;
    const uint32_t bit = eventBit(type);
    portENTER_CRITICAL(&pendingMux);
    pendingMask &= ~bit;
    portEXIT_CRITICAL(&pendingMux);
  }

  uint32_t coalesced() const { return coalescedCount; }
};
