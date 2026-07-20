#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include "../drivers/Display.h"
#include "../drivers/Encoder.h"
#include "../services/RuntimeState.h"
#include "../services/RadioService.h"
#include "../services/RadioStationStore.h"
#include "ThemeManager.h"
#include "Font5x7.h"
#include "../services/RadioArtworkCache.h"

class WebRadioModeUI {
  IQ200Display& display;
  RuntimeState& rt;
  RadioService& radio;
  RadioStationStore& stations;
  ThemeManager themes;
  LGFX_Sprite fb;
  LGFX_Sprite vuFb;
  LGFX_Sprite metaFb;
  LGFX_Sprite artFb;
  bool fbReady = false;
  bool vuFbReady = false;
  bool metaFbReady = false;
  bool artFbReady = false;
  Encoder nav{IQ200_ENC_NAV_CLK, IQ200_ENC_NAV_DT, IQ200_ENC_NAV_SW};
  Encoder volume{IQ200_ENC_VOL_CLK, IQ200_ENC_VOL_DT, IQ200_ENC_VOL_SW};
  uint32_t lastDrawMs = 0;
  uint32_t lastVuDrawMs = 0;
  uint32_t hintUntilMs = 0;
  char lastState[24] = "";
  char lastStation[96] = "";
  char lastTitle[128] = "";
  int lastVolume = -1;
  uint8_t peakLeft = 0;
  uint8_t peakRight = 0;
  uint32_t peakLeftAtMs = 0;
  uint32_t peakRightAtMs = 0;
  bool footerHintDrawn = false;
  uint32_t lastMarqueeMs = 0;
  uint32_t lastArtworkGeneration = 0xFFFFFFFFU;
  uint32_t decodedArtworkGeneration = 0xFFFFFFFFU;
  int stationScroll = 0;
  int artistScroll = 0;
  int titleScroll = 0;
  char marqueeStation[96] = "";
  char marqueeArtist[128] = "";
  char marqueeTitle[128] = "";
  bool stationBrowserOpen = false;
  int stationSelection = 0;
  static constexpr int STATION_ROWS = 7;

  static bool hasUtf8(const char* text) {
    if (!text) return false;
    while (*text) if ((uint8_t)*text++ >= 0x80U) return true;
    return false;
  }

  template <typename Canvas>
  void drawMetadataLine(Canvas& canvas, int x, int y, const char* text,
                        uint16_t fg, uint16_t bg, int scale, int width) {
    if (!text) text = "";
    if (hasUtf8(text)) {
      iqDrawTextClipped(canvas, x, y, text, fg, bg, scale, x, y, width, 7 * scale);
    } else {
      canvas.setTextSize(scale);
      canvas.setTextColor(fg, bg);
      canvas.setCursor(x, y);
      canvas.print(text);
    }
  }

  int metadataWidth(const char* text) const { return hasUtf8(text) ? iqTextWidthPx(text,2) : (int)strlen(text)*12; }

  void pushMarqueeLine(const char* text, int y, int& offset) {
    if(!metaFbReady) return;
    const int width=metadataWidth(text);
    if(width<=340) offset=0; else { offset+=2; if(offset>width+36) offset=-340; }
    metaFb.fillScreen(themes.get().bg);
    if(hasUtf8(text)) iqDrawTextClipped(metaFb,-offset,0,text,themes.get().text,themes.get().bg,2,0,0,340,14);
    else { metaFb.setTextSize(2); metaFb.setTextColor(themes.get().text,themes.get().bg); metaFb.setCursor(-offset,0); metaFb.print(text); }
    display.startWrite(); metaFb.pushSprite(120,y); display.endWrite();
  }

  void drawMarquee(bool force=false) {
    const uint32_t now=millis(); if(!force && now-lastMarqueeMs<100U) return; lastMarqueeMs=now;
    RadioService::Snapshot s; radio.snapshot(s);
    const char* station=s.station[0]?s.station:"Open Web UI to select station";
    String artist=""; String track=s.title[0]?String(s.title):String("—");
    const int separator=track.indexOf(" - ");
    if(separator>0){ artist=track.substring(0,separator); track=track.substring(separator+3); }
    else { artist=station; }
    if(strcmp(marqueeStation,station)!=0){strncpy(marqueeStation,station,sizeof(marqueeStation)-1);marqueeStation[sizeof(marqueeStation)-1]=0;stationScroll=-60;}
    if(strcmp(marqueeArtist,artist.c_str())!=0){strncpy(marqueeArtist,artist.c_str(),sizeof(marqueeArtist)-1);marqueeArtist[sizeof(marqueeArtist)-1]=0;artistScroll=-60;}
    if(strcmp(marqueeTitle,track.c_str())!=0){strncpy(marqueeTitle,track.c_str(),sizeof(marqueeTitle)-1);marqueeTitle[sizeof(marqueeTitle)-1]=0;titleScroll=-60;}
    pushMarqueeLine(marqueeStation,91,stationScroll);
    pushMarqueeLine(marqueeArtist,148,artistScroll);
    pushMarqueeLine(marqueeTitle,177,titleScroll);
  }

  void drawVolumeInfo() {
    RadioService::Snapshot s; radio.snapshot(s);
    display.fillRect(120,203,340,25,themes.get().bg);
    display.setTextSize(1); display.setTextColor(themes.get().dim,themes.get().bg);
    display.setCursor(120,205);
    bool printed=false;
    if(s.codec[0]) { display.print(s.codec); printed=true; }
    if(s.bitrate[0]) { if(printed) display.print("  •  "); display.print(s.bitrate); printed=true; }
    if(s.sampleRate) { if(printed) display.print("  •  "); display.printf("%.1f kHz", s.sampleRate/1000.0f); }
    display.setTextSize(2); display.setTextColor(themes.get().accent,themes.get().bg);
    display.setCursor(20,211); display.printf("VOL %d%%",rt.volumePercent);
    display.setTextSize(1); display.setTextColor(themes.get().ok,themes.get().bg);
    display.setCursor(402,211); display.print(s.state==RadioService::PLAYING?"LIVE":rt.radioStatus);
    lastVolume=rt.volumePercent;
  }

  void drawFooter() {
    const bool hint=hintUntilMs&&(int32_t)(hintUntilMs-millis())>0;
    display.fillRect(0,286,480,34,themes.get().footer);
    display.setTextSize(1); display.setTextColor(themes.get().warn,themes.get().footer); display.setCursor(16,298);
    display.print(hint?"Hold both encoder buttons 2s -> Mode Center":"NAV: stations/select | VOL: volume/stop");
    footerHintDrawn=hint;
  }

  void drawStationBrowser() {
    const int count = stations.count();
    stationSelection = count ? constrain(stationSelection, 0, count - 1) : 0;
    if (!fbReady) {
      display.fillScreen(themes.get().bg);
      display.setTextColor(themes.get().accent, themes.get().bg);
      display.setTextSize(2); display.setCursor(18, 16); display.print("STATIONS");
      display.setTextSize(1); display.setCursor(20, 70); display.print("Station list requires PSRAM framebuffer");
      return;
    }
    fb.fillScreen(themes.get().bg);
    fb.setTextColor(themes.get().accent, themes.get().bg);
    fb.setTextSize(2); fb.setCursor(18, 16); fb.print("WEBRADIO STATIONS");
    fb.setTextSize(1); fb.setTextColor(themes.get().dim, themes.get().bg);
    fb.setCursor(390, 22); fb.printf("%d/%d", count ? stationSelection + 1 : 0, count);
    fb.drawFastHLine(18, 48, 444, themes.get().border);
    if (!count) {
      fb.setTextSize(2); fb.setTextColor(themes.get().warn, themes.get().bg);
      fb.setCursor(54, 132); fb.print("NO STATIONS");
      fb.setTextSize(1); fb.setCursor(54, 162); fb.print("Add stations in Web UI");
    } else {
      int first = constrain(stationSelection - STATION_ROWS / 2, 0, max(0, count - STATION_ROWS));
      for (int row = 0; row < STATION_ROWS && first + row < count; ++row) {
        const int index = first + row;
        const int y = 59 + row * 30;
        const bool selected = index == stationSelection;
        const RadioStation* station = stations.get((uint8_t)index);
        if (!station) continue;
        const uint16_t bg = selected ? themes.get().footer : themes.get().bg;
        const uint16_t fg = selected ? themes.get().accent : themes.get().text;
        fb.fillRoundRect(14, y, 452, 26, 5, bg);
        fb.setTextSize(1); fb.setTextColor(selected ? themes.get().warn : themes.get().dim, bg);
        fb.setCursor(22, y + 9); fb.printf("%02d", index + 1);
        drawMetadataLine(fb, 52, y + 6, station->name.c_str(), fg, bg, 2, 350);
        if (station->favorite) {
          fb.setTextSize(1); fb.setTextColor(themes.get().peak, bg);
          fb.setCursor(438, y + 9); fb.print("*");
        }
      }
    }
    fb.fillRect(0, 286, 480, 34, themes.get().footer);
    fb.setTextSize(1); fb.setTextColor(themes.get().warn, themes.get().footer);
    fb.setCursor(18, 298); fb.print("Rotate NAV: scroll | Press NAV: play");
    display.startWrite(); fb.pushSprite(0, 0); display.endWrite();
  }

  void openStationBrowser() {
    const int count = stations.count();
    if (count && stationSelection >= count) stationSelection = count - 1;
    stationBrowserOpen = true;
    drawStationBrowser();
  }

  void playSelectedStation() {
    const RadioStation* station = stations.get((uint8_t)stationSelection);
    if (!station) { stationBrowserOpen = false; draw(true); return; }
    const String name = station->name, url = station->url, artwork = station->artwork;
    const bool queued = radio.play(name, url, artwork);
    Serial.printf("[WEBRADIO][BROWSER] select=%d/%u queued=%d name=%s\n",
                  stationSelection + 1, stations.count(), queued ? 1 : 0, name.c_str());
    if (queued) {
      Preferences p;
      if (p.begin("iq200-radio-ui", false)) { p.putUChar("station", stationSelection); p.end(); }
      stationBrowserOpen = false;
      draw(true);
    }
  }

  static bool jpegSize(const uint8_t* d,size_t n,int& w,int& h){
    if(!d||n<4||d[0]!=0xFF||d[1]!=0xD8)return false; size_t p=2;
    while(p+9<n){if(d[p++]!=0xFF)continue;uint8_t m=d[p++];if(m==0xD8||m==0xD9)continue;uint16_t len=(d[p]<<8)|d[p+1];if(len<2||p+len>n)return false;
      if((m>=0xC0&&m<=0xC3)||(m>=0xC5&&m<=0xC7)||(m>=0xC9&&m<=0xCB)||(m>=0xCD&&m<=0xCF)){h=(d[p+3]<<8)|d[p+4];w=(d[p+5]<<8)|d[p+6];return w>0&&h>0;}p+=len;}
    return false;
  }

  void drawArtwork(bool force=false){
    RadioArtworkCache& cache=RadioArtworkCache::instance(); const uint32_t gen=cache.generation();
    if(!force&&gen==lastArtworkGeneration)return; lastArtworkGeneration=gen;
    if(!artFbReady) return;
    if(gen!=decodedArtworkGeneration){
      decodedArtworkGeneration=gen; artFb.fillScreen(themes.get().card); bool ok=false;
      RadioArtworkCache::Guard guard(cache); if(guard.locked()&&cache.data()&&cache.size()){
        if(cache.state()==RadioArtworkCache::PNG_READY&&cache.size()>=24){const uint8_t*d=cache.data();int w=((uint32_t)d[16]<<24)|((uint32_t)d[17]<<16)|(d[18]<<8)|d[19];int h=((uint32_t)d[20]<<24)|((uint32_t)d[21]<<16)|(d[22]<<8)|d[23];float sc=min(86.0f/w,94.0f/h);ok=artFb.drawPng(d,cache.size(),0,0,86,94,0,0,sc,sc);}
        else if(cache.state()==RadioArtworkCache::JPEG_READY){int w=0,h=0;if(jpegSize(cache.data(),cache.size(),w,h)){float fit=min(86.0f/w,94.0f/h),sc=fit>=1?1:(fit>=.5f?.5f:(fit>=.25f?.25f:.125f));ok=artFb.drawJpg(cache.data(),cache.size(),0,0,86,96,0,0,sc,sc);}}
      }
      if(!ok){artFb.drawRect(0,0,86,94,themes.get().border);artFb.setTextColor(themes.get().dim,themes.get().card);artFb.setTextSize(2);artFb.setCursor(9,39);artFb.print("RADIO");}
    }
    display.startWrite(); artFb.pushSprite(20,109); display.endWrite();
  }

  uint16_t vuColor(uint8_t segment) const {
    const uint8_t count = constrain(rt.radioVuSegments, 4, 24);
    if (segment * 100U >= count * 86U) return themes.get().vuHigh;
    if (segment * 100U >= count * 66U) return themes.get().vuMid;
    return themes.get().vuLow;
  }

  const char* vuStyleName() const {
    switch (rt.radioVuStyle) { case 0:return "line"; case 1:return "thin"; case 3:return "dot"; case 4:return "neon"; case 5:return "center"; default:return "rect"; }
  }

  void saveSettings() {
    Preferences p;
    if (!p.begin("iq200-radio-ui", false)) return;
    p.putUChar("segments", rt.radioVuSegments); p.putUChar("fps", rt.radioVuFps);
    p.putBool("peak", rt.radioVuPeak); p.putUShort("hold", rt.radioVuHoldMs);
    p.putUChar("decay", rt.radioVuDecay); p.putUChar("style", rt.radioVuStyle);
    p.putUShort("gain", rt.radioVuGain); p.putUChar("gate", rt.radioVuGate);
    p.putUChar("bright", rt.displayBrightness); p.end();
  }

  void loadSettings() {
    themes.begin();
    strncpy(rt.activeTheme, themes.name(), sizeof(rt.activeTheme) - 1);
    rt.activeTheme[sizeof(rt.activeTheme) - 1] = 0;
    Preferences p;
    if (p.begin("iq200-radio-ui", true)) {
      rt.radioVuSegments = constrain(p.getUChar("segments", 24), 4, 24);
      rt.radioVuFps = constrain(p.getUChar("fps", 20), 10, 30);
      rt.radioVuPeak = p.getBool("peak", true);
      rt.radioVuHoldMs = constrain(p.getUShort("hold", 450), 50, 1500);
      rt.radioVuDecay = constrain(p.getUChar("decay", 5), 1, 10);
      rt.radioVuStyle = constrain(p.getUChar("style", 2), 0, 5);
      rt.radioVuGain = constrain(p.getUShort("gain", 100), 50, 300);
      rt.radioVuGate = constrain(p.getUChar("gate", 2), 0, 30);
      rt.displayBrightness = p.getUChar("bright", 255);
      rt.volumePercent = constrain(p.getUChar("volume", 8), 0, 100);
      stationSelection = p.getUChar("station", 0);
      p.end();
    }
    display.setBrightness(rt.displayBrightness);
  }

  template <typename Canvas>
  void drawVuCanvas(Canvas& canvas, uint8_t left, uint8_t right) {
    canvas.fillScreen(themes.get().bg);
    const uint8_t levels[2] = {left, right};
    const uint8_t peaks[2] = {peakLeft, peakRight};
    for (uint8_t channel = 0; channel < 2; ++channel) {
      const int y = channel ? 27 : 6;
      canvas.setTextSize(1);
      canvas.setTextColor(themes.get().accent, themes.get().bg);
      canvas.setCursor(0, y + 3);
      canvas.print(channel ? "R" : "L");
      const uint8_t count = constrain(rt.radioVuSegments, 4, 24);
      const uint8_t active = (uint8_t)(((uint16_t)levels[channel] * count + 254U) / 255U);
      const uint8_t peakSegment = peaks[channel] ? (uint8_t)(((uint16_t)peaks[channel] * (count - 1U)) / 255U) : 255;
      const int meterX = 20, meterW = 408;

      // LINE: a continuous analogue bar with a dim rail and bright level line.
      if (rt.radioVuStyle == 0) {
        const int pixels = ((uint32_t)levels[channel] * meterW) / 255U;
        canvas.drawFastHLine(meterX, y + 6, meterW, themes.get().card);
        canvas.drawFastHLine(meterX, y + 7, meterW, themes.get().border);
        if (pixels) {
          canvas.drawFastHLine(meterX, y + 6, pixels, vuColor(active >= count ? count - 1 : active));
          canvas.drawFastHLine(meterX, y + 7, pixels, themes.get().accent);
        }
        if (rt.radioVuPeak && peaks[channel]) {
          const int px = meterX + ((uint32_t)peaks[channel] * (meterW - 1)) / 255U;
          canvas.drawFastVLine(px, y + 3, 10, themes.get().peak);
        }
        continue;
      }

      const int step = meterW / count;
      for (uint8_t segment = 0; segment < count; ++segment) {
        const int x = meterX + segment * step;
        const int width = max(2, step - 3);
        const uint16_t color = segment < active ? vuColor(segment) : themes.get().card;

        // THIN: narrow vertical needles with plenty of negative space.
        if (rt.radioVuStyle == 1) {
          const int needleW = max(1, min(3, width));
          canvas.fillRect(x + (width - needleW) / 2, y + 1, needleW, 13, color);
        }
        // BLOCKS: solid wide rectangular LED segments.
        else if (rt.radioVuStyle == 2) {
          canvas.fillRect(x, y, width, 14, color);
        }
        // DOTS: circular LED points.
        else if (rt.radioVuStyle == 3) {
          const int radius = max(1, min(6, width / 2));
          canvas.fillCircle(x + width / 2, y + 7, radius, color);
        }
        // NEON: luminous core plus two bounded glow outlines.
        else if (rt.radioVuStyle == 4) {
          if (segment < active) {
            canvas.drawRect(x, y + 1, width, 12, themes.get().border);
            if (width > 4) canvas.drawRect(x + 1, y + 2, width - 2, 10, color);
            if (width > 6) canvas.fillRect(x + 3, y + 4, width - 6, 6, themes.get().text);
          } else {
            canvas.drawRect(x, y + 3, width, 8, themes.get().card);
          }
        }
        // CENTER: mirrored blocks grow from the middle toward both edges.
        else {
          const int halfW = meterW / 2;
          const int halfStep = halfW / count;
          const int blockW = max(1, halfStep - 2);
          const int centerX = meterX + halfW;
          const int dx = segment * halfStep;
          canvas.fillRect(centerX + dx + 1, y + 2, blockW, 10, color);
          canvas.fillRect(centerX - dx - blockW - 1, y + 2, blockW, 10, color);
        }

        if (rt.radioVuPeak && segment == peakSegment && peaks[channel]) {
          if (rt.radioVuStyle == 5) {
            const int halfStep = (meterW / 2) / count;
            const int dx = segment * halfStep;
            canvas.drawFastVLine(meterX + meterW / 2 + dx, y, 14, themes.get().peak);
            canvas.drawFastVLine(meterX + meterW / 2 - dx, y, 14, themes.get().peak);
          } else canvas.drawFastVLine(x + width - 1, y, 14, themes.get().peak);
        }
      }
    }
  }

  void drawVu(bool force = false) {
    const uint32_t now = millis();
    if (!force && now - lastVuDrawMs < 1000U / constrain(rt.radioVuFps, 10, 30)) return;
    lastVuDrawMs = now;
    const uint8_t left = rt.radioVuLeft;
    const uint8_t right = rt.radioVuRight;
    if (left >= peakLeft) { peakLeft = left; peakLeftAtMs = now; }
    else if (now - peakLeftAtMs > rt.radioVuHoldMs) peakLeft = peakLeft > rt.radioVuDecay ? peakLeft - rt.radioVuDecay : 0;
    if (right >= peakRight) { peakRight = right; peakRightAtMs = now; }
    else if (now - peakRightAtMs > rt.radioVuHoldMs) peakRight = peakRight > rt.radioVuDecay ? peakRight - rt.radioVuDecay : 0;

    if (!vuFbReady) return;
    drawVuCanvas(vuFb, left, right);
    display.startWrite();
    vuFb.pushSprite(20, 232);
    display.endWrite();
  }

  template <typename Canvas>
  void drawCanvas(Canvas& canvas, const RadioService::Snapshot& snapshot) {
    canvas.fillScreen(themes.get().bg);
    canvas.setTextColor(themes.get().accent, themes.get().bg);
    canvas.setTextSize(2); canvas.setCursor(18, 13);
    canvas.print(snapshot.station[0] ? snapshot.station : "WEBRADIO");
    canvas.setTextSize(1); canvas.setTextColor(themes.get().dim,themes.get().bg);
    canvas.setCursor(394,20); canvas.print(rt.webIp[0]?rt.webIp:"OFFLINE");
    canvas.drawFastHLine(18, 42, 444, themes.get().border);

    canvas.setTextSize(1); canvas.setTextColor(themes.get().dim,themes.get().bg);
    canvas.setCursor(20,55); canvas.print("STATION");
    canvas.fillRect(120,91,340,14,themes.get().bg);

    canvas.fillRoundRect(16,105,94,102,7,themes.get().card);
    canvas.drawRoundRect(16,105,94,102,7,themes.get().border);

    canvas.setTextSize(1); canvas.setTextColor(themes.get().dim,themes.get().bg);
    canvas.setCursor(120,126); canvas.print("ARTIST");
    canvas.fillRect(120,148,340,14,themes.get().bg);
    canvas.setCursor(120,166); canvas.print("TRACK");
    canvas.fillRect(120,177,340,14,themes.get().bg);

    canvas.setTextSize(2); canvas.setTextColor(themes.get().accent,themes.get().bg);
    canvas.setCursor(20,211); canvas.printf("VOL %d%%",rt.volumePercent);
    canvas.setTextSize(1); canvas.setTextColor(snapshot.state==RadioService::PLAYING?themes.get().ok:themes.get().warn,themes.get().bg);
    canvas.setCursor(402,211); canvas.print(snapshot.state==RadioService::PLAYING?"LIVE":rt.radioStatus);
    canvas.setTextColor(themes.get().dim,themes.get().bg); canvas.setCursor(120,205);
    bool printed=false;
    if(snapshot.codec[0]) { canvas.print(snapshot.codec); printed=true; }
    if(snapshot.bitrate[0]) { if(printed) canvas.print("  •  "); canvas.print(snapshot.bitrate); printed=true; }
    if(snapshot.sampleRate) { if(printed) canvas.print("  •  "); canvas.printf("%.1f kHz", snapshot.sampleRate/1000.0f); }

    canvas.fillRect(20,232,440,48,themes.get().bg);
    canvas.fillRect(0,286,480,34,themes.get().footer);
    canvas.setTextSize(1); canvas.setTextColor(themes.get().warn,themes.get().footer);
    canvas.setCursor(16,298);
    canvas.print(hintUntilMs&&(int32_t)(hintUntilMs-millis())>0 ? "Hold both encoder buttons 2s -> Mode Center" : "NAV: stations/select | VOL: volume/stop");
  }

  void draw(bool force = false) {
    RadioService::Snapshot snapshot;
    radio.snapshot(snapshot);
    const bool changed = force || strcmp(lastState, rt.radioStatus) != 0 ||
                         strcmp(lastStation, snapshot.station) != 0;
    if (!changed) return;
    lastDrawMs = millis();
    strncpy(lastState, rt.radioStatus, sizeof(lastState) - 1);
    lastState[sizeof(lastState) - 1] = 0;
    strncpy(lastStation, snapshot.station, sizeof(lastStation) - 1);
    lastStation[sizeof(lastStation) - 1] = 0;

    if (!fbReady) {
      drawCanvas(display, snapshot);
      return;
    }
    drawCanvas(fb, snapshot);
    display.startWrite();
    fb.pushSprite(0, 0);
    display.endWrite();
    drawArtwork(true);
    drawMarquee(true);
    drawVu(true);
  }

public:
  WebRadioModeUI(IQ200Display& targetDisplay, RuntimeState& state, RadioService& radioService,
                 RadioStationStore& stationStore)
      : display(targetDisplay), rt(state), radio(radioService), stations(stationStore),
        fb(&targetDisplay), vuFb(&targetDisplay), metaFb(&targetDisplay), artFb(&targetDisplay) {}

  void begin() {
    nav.begin();
    volume.begin();
    loadSettings();
    fb.setPsram(true);
    fb.setColorDepth(16);
    fbReady = fb.createSprite(480, 320) != nullptr;
    vuFb.setPsram(true);
    vuFb.setColorDepth(16);
    vuFbReady = vuFb.createSprite(440, 48) != nullptr;
    metaFb.setPsram(true); metaFb.setColorDepth(16); metaFbReady=metaFb.createSprite(340,18)!=nullptr;
    artFb.setPsram(true); artFb.setColorDepth(16); artFbReady=artFb.createSprite(86,94)!=nullptr;
    Serial.printf("[WEBRADIO_UI] framebuffer=%s bytes=%u\n",
                  fbReady ? "PSRAM_OK" : "DIRECT_FALLBACK",
                  fbReady ? 480U * 320U * 2U : 0U);
    Serial.printf("[WEBRADIO_UI] vu=%s bytes=%u fps=%u\n",
                  vuFbReady ? "PSRAM_OK" : "DISABLED",
                  vuFbReady ? 440U * 48U * 2U : 0U, rt.radioVuFps);
    draw(true);
  }

  bool handleCommand(String command) {
    command.trim(); command.toLowerCase();
    if (command.startsWith("theme ")) {
      if (!themes.set(command.substring(6), true)) return false;
      strncpy(rt.activeTheme, themes.name(), sizeof(rt.activeTheme) - 1);
      rt.activeTheme[sizeof(rt.activeTheme) - 1] = 0; draw(true); return true;
    }
    if (command.startsWith("brightness ")) {
      const int value = command.substring(11).toInt(); if (value < 5 || value > 255) return false;
      rt.displayBrightness = value; display.setBrightness(value); saveSettings(); return true;
    }
    if (command.startsWith("vuseg ")) {
      const int value = command.substring(6).toInt(); if (value < 4 || value > 24) return false;
      rt.radioVuSegments = value; saveSettings(); drawVu(true); return true;
    }
    if (command.startsWith("vu fps ")) {
      const int value = command.substring(7).toInt(); if (value < 10 || value > 30) return false;
      rt.radioVuFps = value; saveSettings(); return true;
    }
    if (command.startsWith("vu style ")) {
      const String style = command.substring(9);
      if (style == "line") rt.radioVuStyle=0; else if (style == "thin") rt.radioVuStyle=1;
      else if (style == "rect" || style == "blocks") rt.radioVuStyle=2; else if (style == "dot") rt.radioVuStyle=3;
      else if (style == "neon") rt.radioVuStyle=4; else if (style == "center") rt.radioVuStyle=5; else return false;
      saveSettings(); drawVu(true); return true;
    }
    if (command.startsWith("vu peak ")) {
      const String value=command.substring(8); if(value!="on"&&value!="off") return false;
      rt.radioVuPeak=value=="on"; saveSettings(); drawVu(true); return true;
    }
    if (command.startsWith("vu hold ")) {
      const int value=command.substring(8).toInt(); if(value<50||value>1500) return false;
      rt.radioVuHoldMs=value; saveSettings(); return true;
    }
    if (command.startsWith("vu decay ")) {
      const int value=command.substring(9).toInt(); if(value<1||value>10) return false;
      rt.radioVuDecay=value; saveSettings(); return true;
    }
    if (command.startsWith("vu gain ")) {
      const int value=command.substring(8).toInt(); if(value<50||value>300) return false;
      rt.radioVuGain=value; saveSettings(); return true;
    }
    if (command.startsWith("vu gate ")) {
      const int value=command.substring(8).toInt(); if(value<0||value>30) return false;
      rt.radioVuGate=value; saveSettings(); return true;
    }
    if (command == "vu status") {
      Serial.printf("[WEBRADIO][VU] style=%s segments=%u fps=%u peak=%d hold=%u decay=%u gain=%u gate=%u\n",
                    vuStyleName(), rt.radioVuSegments, rt.radioVuFps, rt.radioVuPeak?1:0, rt.radioVuHoldMs, rt.radioVuDecay, rt.radioVuGain, rt.radioVuGate);
      return true;
    }
    if (command == "eq" || command == "eq status") { radio.printEqualizer(); return true; }
    if (command.startsWith("eq custom ")) {
      int b=0,m=0,t=0; if (sscanf(command.c_str()+10, "%d %d %d", &b,&m,&t)!=3) return false;
      return radio.setEqualizerCustom(b,m,t);
    }
    if (command.startsWith("eq ")) return radio.setEqualizerPreset(command.substring(3));
    return false;
  }

  void setVolume(int value) {
    rt.volumePercent = constrain(value, 0, 100);
    Preferences p;
    if (p.begin("iq200-radio-ui", false)) { p.putUChar("volume", rt.volumePercent); p.end(); }
    drawVolumeInfo();
  }

  void tick() {
    const int delta = volume.delta(false, 2);
    if (delta) {
      setVolume(rt.volumePercent + delta * 2);
    }
    if (volume.pressed()) {
      radio.stop();
    }
    const int navDelta = nav.delta(false, 2);
    if (navDelta) {
      const int count = stations.count();
      if (count) {
        if (!stationBrowserOpen) stationBrowserOpen = true;
        stationSelection = (stationSelection + navDelta + count) % count;
        drawStationBrowser();
      }
    }
    if (nav.pressed()) {
      if (stationBrowserOpen) playSelectedStation();
      else openStationBrowser();
    }
    if (stationBrowserOpen) return;
    draw(false);
    const bool hintNow=hintUntilMs&&(int32_t)(hintUntilMs-millis())>0;
    if(hintNow!=footerHintDrawn) drawFooter();
    drawArtwork(false);
    drawMarquee(false);
    if (millis() - lastVuDrawMs >= 1000U / constrain(rt.radioVuFps, 10, 30)) drawVu(false);
  }
};
