#pragma once
#include <Arduino.h>

struct UIRect { int16_t x; int16_t y; int16_t w; int16_t h; };

enum PlayerLayoutMode : uint8_t {
  PLAYER_LAYOUT_MODERN = 0,
  PLAYER_LAYOUT_DUAL_VU = 1,
  PLAYER_LAYOUT_ARTWORK = 2,
  PLAYER_LAYOUT_CLASSIC = 3
};

struct PlayerLayout {
  UIRect content;
  UIRect title;
  UIRect state;
  UIRect volume;
  UIRect vuLeft;
  UIRect vuRight;
  UIRect progress;
  UIRect progressText;
  UIRect artwork;
  UIRect playButton;
  UIRect stopButton;
  UIRect backButton;
  int16_t separatorY;
};

inline const PlayerLayout& iq200PlayerLayout(PlayerLayoutMode mode) {
  // All layouts fit inside 480x320, below the 48 px status/header area.
  static const PlayerLayout modern = {
    {0,48,480,272}, {192,62,272,34}, {192,102,170,24}, {366,102,98,24},
    {192,170,272,12}, {192,188,272,12}, {16,232,392,18}, {414,230,50,22},
    {16,62,160,160}, {40,282,112,30}, {184,282,112,30}, {328,282,112,30}, 52
  };
  static const PlayerLayout dual = {
    {0,48,480,272}, {192,62,272,34}, {192,102,170,24}, {366,102,98,24},
    {16,220,448,14}, {16,240,448,14}, {16,264,392,16}, {414,262,50,20},
    {16,54,160,160}, {40,286,112,28}, {184,286,112,28}, {328,286,112,28}, 52
  };
  static const PlayerLayout artwork = {
    {0,48,480,272}, {222,62,242,34}, {222,104,146,24}, {370,104,94,24},
    {222,170,242,12}, {222,188,242,12}, {72,244,336,16}, {414,242,50,20},
    {16,54,190,180}, {40,282,112,30}, {184,282,112,30}, {328,282,112,30}, 52
  };
  static const PlayerLayout classic = {
    {0,48,480,272}, {144,60,320,34}, {144,102,200,24}, {350,102,114,24},
    {16,180,448,18}, {16,204,448,18}, {16,236,392,18}, {414,234,50,22},
    {16,56,112,112}, {40,282,112,30}, {184,282,112,30}, {328,282,112,30}, 52
  };
  switch (mode) {
    case PLAYER_LAYOUT_DUAL_VU: return dual;
    case PLAYER_LAYOUT_ARTWORK: return artwork;
    case PLAYER_LAYOUT_CLASSIC: return classic;
    default: return modern;
  }
}
