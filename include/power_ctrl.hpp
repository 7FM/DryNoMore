#pragma once

#include <Arduino.h>

struct ShiftReg {
  ShiftReg() {
    init();
    update(0);
  }

  void enableOutput() const;
  void disableOutput() const;
  void update(uint16_t newVal) const;

private:
  void init() const;
};
