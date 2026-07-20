#pragma once
#include <Arduino.h>
#include "../services/AppManager.h"

class WindowManager {
  static const int STACK_MAX = 8;
  IQAppId stack[STACK_MAX];
  int top = -1;

public:
  void reset() { top = -1; }

  void push(IQAppId app) {
    if (top < STACK_MAX - 1) {
      stack[++top] = app;
    } else {
      for (int i = 1; i < STACK_MAX; i++) stack[i - 1] = stack[i];
      stack[STACK_MAX - 1] = app;
    }
  }

  bool canBack() const { return top > 0; }

  IQAppId back() {
    if (top > 0) top--;
    return current();
  }

  IQAppId current() const {
    if (top < 0) return APP_STATUS;
    return stack[top];
  }

  int depth() const { return top + 1; }
};
