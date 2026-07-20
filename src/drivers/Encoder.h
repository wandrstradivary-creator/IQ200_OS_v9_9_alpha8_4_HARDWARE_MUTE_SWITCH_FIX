#pragma once
#include <Arduino.h>

class Encoder {
public:
  int a, b, sw;
  int lastA = 1;
  int acc = 0;
  bool lastSW = true;
  uint32_t lm = 0;
  uint32_t lp = 0;

  Encoder(int A, int B, int S) : a(A), b(B), sw(S) {}

  void begin() {
    pinMode(a, INPUT_PULLUP);
    pinMode(b, INPUT_PULLUP);
    pinMode(sw, INPUT_PULLUP);
    lastA = digitalRead(a);
    lastSW = digitalRead(sw);
  }

  int delta(bool invert = false, int detent = 2) {
    int A = digitalRead(a);
    int B = digitalRead(b);
    int out = 0;
    if (A != lastA) {
      if (millis() - lm > 2) {
        int st = (A == B) ? 1 : -1;
        if (invert) st = -st;
        acc += st;
        if (abs(acc) >= detent) {
          out = acc > 0 ? 1 : -1;
          acc = 0;
        }
        lm = millis();
      }
      lastA = A;
    }
    return out;
  }

  bool pressed() {
    bool v = digitalRead(sw);
    bool e = false;
    if (lastSW && !v && millis() - lp > 250) {
      e = true;
      lp = millis();
    }
    lastSW = v;
    return e;
  }
};
