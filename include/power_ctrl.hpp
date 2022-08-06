#pragma once

#include <Arduino.h>

struct ShiftReg {
  ShiftReg() {
    init();
    update(0);
  }

  void enableOutput();
  void disableOutput();
  void update(uint16_t newVal);

private:
  void init();
};
