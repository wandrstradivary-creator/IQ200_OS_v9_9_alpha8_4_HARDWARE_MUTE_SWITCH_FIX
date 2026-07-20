#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <Audio.h>
#include <new>
#include "RuntimeState.h"
#include "IQPlayerCore.h"
#include "AudioEngine.h"
#include "iq200_pins.h"

// WebRadio playback engine based on ESP32-audioI2S.
// HTTP/HTTPS -> MP3/AAC decoder -> I2S/PCM5102.
//
// v9.9-alpha4 contract:
// - REST/UI callers only enqueue commands and never perform DNS/HTTP/I2S work.
// - Only webradio_rt calls Audio methods.
// - Cross-task status is exposed as a fixed-size snapshot, never String refs.
class RadioService {
public:
  static RadioService* activeInstance;
  enum State : uint8_t {
    IDLE,
    QUEUED,
    CONNECTING,
    BUFFERING,
    PLAYING,
    RECONNECTING,
    ERROR_STATE,
    STOPPED
  };

  struct Snapshot {
    State state = IDLE;
    bool active = false;
    bool sessionStarted = false;
    char station[96] = "";
    char url[241] = "";
    char title[128] = "";
    char artwork[241] = "";
    char error[128] = "";
    char codec[12] = "";
    char bitrate[24] = "";
    uint32_t sampleRate = 0;
    uint32_t uptimeSeconds = 0;
    uint32_t commandsQueued = 0;
    uint32_t commandsDropped = 0;
    uint32_t connectAttempts = 0;
    uint32_t reconnects = 0;
    uint32_t unsupportedStreams = 0;
  bool hardwareMuted = true;
  bool unmutePending = false;
  uint32_t unmuteAtMs = 0;
    uint32_t taskStackHighWater = 0;
  };

private:
  enum CommandType : uint8_t { CMD_PLAY = 1, CMD_STOP = 2 };
  struct Command {
    CommandType type = CMD_STOP;
    char station[81] = "";
    char url[241] = "";
    char artwork[241] = "";
  };

  RuntimeState* rt = nullptr;
  IQPlayerCore* localPlayer = nullptr;
  AudioEngine* localAudio = nullptr;
  // ESP32-audioI2S owns sizeable stream/decoder buffers. Allocate it only when
  // WEBRADIO is the selected clean-boot platform; Local Player and Mode Center
  // must not pay that RAM cost merely because RadioService is a global object.
  Audio* radio = nullptr;
  TaskHandle_t taskHandle = nullptr;
  QueueHandle_t commandQueue = nullptr;
  mutable portMUX_TYPE snapshotMux = portMUX_INITIALIZER_UNLOCKED;
  Snapshot cached;

  volatile bool running = false;
  volatile bool radioSessionStarted = false;
  volatile bool reconnectRequested = false;
  volatile bool stopCommandPending = false;
  volatile bool fatalStreamErrorPending = false;
  volatile State state = IDLE;
  volatile uint8_t requestedVolumePercent = 8;
  uint8_t appliedVolumePercent = 255;
  volatile bool eqUpdatePending = false;
  volatile bool requestedEqEnabled = true;
  volatile int8_t requestedEqBass = 0;
  volatile int8_t requestedEqMid = 0;
  volatile int8_t requestedEqTreble = 0;
  int8_t appliedEqBass = 127;
  int8_t appliedEqMid = 127;
  int8_t appliedEqTreble = 127;

  char stationName[96] = "";
  char streamUrl[241] = "";
  char streamTitle[128] = "";
  char artworkUrl[241] = "";
  char lastError[128] = "";
  char codecName[12] = "";
  char bitrateText[24] = "";
  uint32_t streamSampleRate = 0;
  uint16_t vuAgcPeak = 64;
  uint32_t startedMs = 0;
  uint32_t lastDataMs = 0;
  uint32_t reconnectAtMs = 0;
  uint32_t lastMirrorMs = 0;
  uint32_t lastVuSampleMs = 0;
  uint8_t vuLeft = 0;
  uint8_t vuRight = 0;
  volatile uint8_t pcmVuLeft = 0;
  volatile uint8_t pcmVuRight = 0;
  uint8_t reconnectAttempt = 0;
  uint32_t commandsQueued = 0;
  uint32_t commandsDropped = 0;
  uint32_t connectAttempts = 0;
  uint32_t reconnects = 0;
  uint32_t unsupportedStreams = 0;
  bool hardwareMuted = true;
  bool unmutePending = false;
  uint32_t unmuteAtMs = 0;

  static void taskEntry(void* arg);
  void taskLoop();
  void handleCommand(const Command& command);
  void handlePlay(const Command& command);
  void handleStop();
  bool connectNow();
  void applyVolume();
  void applyEqualizer();
  void sampleVu();
  void resetVu();
  void setHardwareMute(bool muted);
  void scheduleHardwareUnmute(uint32_t delayMs = 100);
  void serviceHardwareMute();
  void mirror();
  void setState(State next, const char* error = nullptr);
  static const char* stateName(State s);
  static void copyText(char* dst, size_t len, const char* src);
  static String cleanMetadata(const char* src);
  void parseStreamInfo(const char* info);

public:
  void begin(RuntimeState& stateRef, IQPlayerCore& playerRef, AudioEngine& audioRef);
  bool play(const String& name, const String& url, const String& artwork = "");  // asynchronous enqueue
  bool stop();                                       // asynchronous enqueue
  void tick();
  bool setEqualizerPreset(const String& preset);
  bool setEqualizerCustom(int bass, int mid, int treble);
  void printEqualizer() const;
  bool isPlaying() const { return state == PLAYING || state == BUFFERING; }
  bool isActive() const {
    const State s = state;
    return s == QUEUED || s == CONNECTING || s == BUFFERING || s == PLAYING || s == RECONNECTING ||
           running || radioSessionStarted;
  }
  State getState() const { return state; }
  void snapshot(Snapshot& out) const;
  void print() const;

  // ESP32-audioI2S callbacks forward here. They run in webradio_rt context.
  void onInfo(const char* info);
  void onStation(const char* info);
  void onStreamTitle(const char* info);
  void onBitrate(const char* info);
  void onEof(const char* info);
  void onPcmLevel(uint8_t left, uint8_t right);
};
