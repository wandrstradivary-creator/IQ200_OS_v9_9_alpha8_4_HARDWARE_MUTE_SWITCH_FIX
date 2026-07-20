#pragma once

#define IQ200_TFT_MOSI      11
#define IQ200_TFT_MISO      13
#define IQ200_TFT_SCLK      12
#define IQ200_TFT_CS        10
#define IQ200_TFT_DC        9
#define IQ200_TFT_RST       -1
#define IQ200_TFT_BL        14

#define IQ200_SD_CS         38

#define IQ200_ENC_NAV_CLK   42
#define IQ200_ENC_NAV_DT    2
#define IQ200_ENC_NAV_SW    41

#define IQ200_ENC_VOL_CLK   5
#define IQ200_ENC_VOL_DT    6
#define IQ200_ENC_VOL_SW    7

#define IQ200_I2S_BCK       16
#define IQ200_I2S_LRCK      18
#define IQ200_I2S_DOUT      17

// External amplifier/DAC hardware mute. Active LOW.
#ifndef MUTE_PIN
#define MUTE_PIN             4
#endif
#ifndef MUTE_VAL
#define MUTE_VAL             LOW
#endif
