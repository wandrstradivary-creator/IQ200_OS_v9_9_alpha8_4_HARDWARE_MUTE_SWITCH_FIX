#pragma once
#include <Arduino.h>
#include <HTTPClient.h>
#include <esp_heap_caps.h>

// Single-image PSRAM cache for WebRadio. fetch() is called only after the old
// station is stopped and before the new audio connection starts, so cover HTTP
// traffic can never compete with active streaming.
class RadioArtworkCache {
public:
  enum State : uint8_t { EMPTY=0, JPEG_READY=1, PNG_READY=2, SKIPPED=3, ERROR_STATE=4 };
  class Guard {
    RadioArtworkCache& owner; bool ok;
  public:
    explicit Guard(RadioArtworkCache& cache, TickType_t wait=pdMS_TO_TICKS(20)) : owner(cache), ok(owner.lock(wait)) {}
    ~Guard(){ if(ok) owner.unlock(); }
    bool locked() const { return ok; }
  };

  static RadioArtworkCache& instance(){ static RadioArtworkCache cache; return cache; }

  bool fetch(const char* sourceUrl) {
    if (!clear()) return false;
    String url = sourceUrl ? sourceUrl : ""; url.trim();
    if (!url.length()) { state_ = SKIPPED; generation_++; return false; }
    if ((!url.startsWith("http://") && !url.startsWith("https://")) || url.length() > 240) {
      state_ = ERROR_STATE; generation_++; return false;
    }
    HTTPClient http;
    http.setConnectTimeout(1200); http.setTimeout(1600);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    if (!http.begin(url)) { state_ = ERROR_STATE; generation_++; return false; }
    const int code = http.GET();
    if (code != HTTP_CODE_OK) { http.end(); state_ = ERROR_STATE; generation_++; return false; }
    static constexpr size_t MAX_BYTES = 256U * 1024U;
    const int announced = http.getSize();
    if (announced > (int)MAX_BYTES) { http.end(); state_ = ERROR_STATE; generation_++; return false; }
    uint8_t* next = (uint8_t*)heap_caps_malloc(MAX_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!next) { http.end(); state_ = ERROR_STATE; generation_++; return false; }
    WiFiClient* stream = http.getStreamPtr();
    size_t total = 0; uint32_t idleSince = millis();
    while (http.connected() && total < MAX_BYTES) {
      const size_t available = stream->available();
      if (available) {
        const size_t chunk = min<size_t>(available, min<size_t>(2048, MAX_BYTES-total));
        const int got = stream->readBytes(next+total, chunk);
        if (got > 0) { total += got; idleSince = millis(); }
      } else {
        if ((announced >= 0 && total >= (size_t)announced) || millis()-idleSince > 350U) break;
        vTaskDelay(pdMS_TO_TICKS(2));
      }
    }
    http.end();
    const bool jpeg = total >= 3 && next[0]==0xFF && next[1]==0xD8 && next[2]==0xFF;
    const bool png = total >= 8 && next[0]==0x89 && next[1]==0x50 && next[2]==0x4E && next[3]==0x47;
    if (!jpeg && !png) { free(next); state_=ERROR_STATE; generation_++; return false; }
    if (!lock(pdMS_TO_TICKS(50))) { free(next); state_=ERROR_STATE; generation_++; return false; }
    data_=next; size_=total; state_=jpeg?JPEG_READY:PNG_READY; url_=url; generation_++;
    unlock();
    Serial.printf("[RADIO_ART] ready type=%s bytes=%u\n", jpeg?"JPEG":"PNG", (unsigned)total);
    return true;
  }

  bool clear(){
    if(!lock(pdMS_TO_TICKS(50))) return false;
    if(data_) free(data_); data_=nullptr; size_=0; url_=""; state_=EMPTY; generation_++; unlock(); return true;
  }
  const uint8_t* data() const { return data_; }
  size_t size() const { return size_; }
  State state() const { return state_; }
  uint32_t generation() const { return generation_; }
  const String& url() const { return url_; }

private:
  SemaphoreHandle_t mutex_=xSemaphoreCreateMutex();
  uint8_t* data_=nullptr; size_t size_=0; State state_=EMPTY; uint32_t generation_=0; String url_;
  bool lock(TickType_t wait){ return mutex_ && xSemaphoreTake(mutex_,wait)==pdTRUE; }
  void unlock(){ xSemaphoreGive(mutex_); }
};
