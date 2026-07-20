#pragma once
#include <Arduino.h>
#include "RuntimeState.h"

// IQ200 OS v8.0.5 Media Pipeline foundation.
// Lightweight diagnostics-only pipeline layer. It does not touch RT Audio.
class MediaPipeline {
public:
  void begin(RuntimeState& state) {
    _rt = &state;
    reset();
  }

  void reset() {
    if (!_rt) return;
    _rt->pipelineTicks = 0;
    _rt->pipelineDropped = 0;
    _rt->pipelineFileQueueDepth = 0;
    _rt->pipelineFileQueueSize = FILE_Q_SIZE;
    _rt->pipelineMetaQueueDepth = 0;
    _rt->pipelineMetaQueueSize = META_Q_SIZE;
    _rt->pipelineDbQueueDepth = 0;
    _rt->pipelineDbQueueSize = DB_Q_SIZE;
    _rt->pipelineParserOk = 1;
    _rt->pipelineWriterOk = 1;
    _rt->pipelineAlbumArtOk = 1;
  }

  void tick() {
    if (!_rt) return;
    _rt->pipelineTicks++;

    // Mirror real system pressure into harmless queue-depth diagnostics.
    // These are non-blocking counters for v8.0.5 and become real queues later.
    uint32_t fileDepth = 0;
    if (_rt->scanLock || _rt->dbScanBusy) {
      fileDepth = (uint32_t)(_rt->scanFiles % (FILE_Q_SIZE + 1));
    }
    _rt->pipelineFileQueueDepth = fileDepth;
    _rt->pipelineMetaQueueDepth = fileDepth > 2 ? fileDepth / 2 : 0;
    _rt->pipelineDbQueueDepth = fileDepth > 4 ? fileDepth / 4 : 0;

    _rt->pipelineParserOk = 1;
    _rt->pipelineWriterOk = 1;
    _rt->pipelineAlbumArtOk = 1;
  }

  void print() const {
    if (!_rt) return;
    Serial.println("=== MEDIA PIPELINE v8.0.5 ===");
    Serial.printf("FileQueue : %lu/%lu\n", (unsigned long)_rt->pipelineFileQueueDepth, (unsigned long)_rt->pipelineFileQueueSize);
    Serial.printf("MetaQueue : %lu/%lu\n", (unsigned long)_rt->pipelineMetaQueueDepth, (unsigned long)_rt->pipelineMetaQueueSize);
    Serial.printf("DBQueue   : %lu/%lu\n", (unsigned long)_rt->pipelineDbQueueDepth, (unsigned long)_rt->pipelineDbQueueSize);
    Serial.printf("Parser    : %s\n", _rt->pipelineParserOk ? "OK" : "FAIL");
    Serial.printf("Writer    : %s\n", _rt->pipelineWriterOk ? "OK" : "FAIL");
    Serial.printf("AlbumArt  : %s\n", _rt->pipelineAlbumArtOk ? "OK" : "FAIL");
    Serial.printf("Dropped   : %lu\n", (unsigned long)_rt->pipelineDropped);
    Serial.printf("Events    : posts=%lu drops=%lu media=%lu queue=%lu db=%lu scan=%lu\n",
                  (unsigned long)_rt->eventBusPosts,
                  (unsigned long)_rt->eventBusDrops,
                  (unsigned long)_rt->mediaEventCount,
                  (unsigned long)_rt->queueEventCount,
                  (unsigned long)_rt->dbEventCount,
                  (unsigned long)_rt->scanEventCount);
    Serial.printf("Ticks     : %lu\n", (unsigned long)_rt->pipelineTicks);
  }

private:
  static constexpr uint32_t FILE_Q_SIZE = 64;
  static constexpr uint32_t META_Q_SIZE = 32;
  static constexpr uint32_t DB_Q_SIZE = 32;
  RuntimeState* _rt = nullptr;
};
