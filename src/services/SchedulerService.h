#pragma once
#include <Arduino.h>

struct SchedulerStats {
  volatile uint32_t audioTicks = 0;
  volatile uint32_t sdTicks = 0;
  volatile uint32_t wifiTicks = 0;
  volatile uint32_t otaTicks = 0;
  volatile uint32_t nvsTicks = 0;
  volatile uint32_t guiTicks = 0;
  volatile uint32_t inputTicks = 0;
  volatile uint32_t animationTicks = 0;
  volatile uint32_t notificationTicks = 0;
};

class SchedulerService {
public:
  SchedulerStats stats;

  void tickAudio() { stats.audioTicks++; }
  void tickSD() { stats.sdTicks++; }
  void tickWiFi() { stats.wifiTicks++; }
  void tickOTA() { stats.otaTicks++; }
  void tickNVS() { stats.nvsTicks++; }

  void tickGUI() { stats.guiTicks++; }
  void tickInput() { stats.inputTicks++; }
  void tickAnimation() { stats.animationTicks++; }
  void tickNotification() { stats.notificationTicks++; }
};
