#pragma once
#include <Arduino.h>

class RendererStats {
public:
  uint32_t frameCount = 0;
  uint32_t dirtyFrames = 0;
  uint32_t partialFrames = 0;
  uint32_t fullFrames = 0;
  uint32_t lastFrameMs = 0;
  uint32_t lastDirtyMs = 0;

  void beginFrame() { lastFrameMs = millis(); }
  void endFrame(bool full = true) {
    frameCount++;
    if (full) fullFrames++;
    else partialFrames++;
  }
  void dirty() {
    dirtyFrames++;
    lastDirtyMs = millis();
  }
};
