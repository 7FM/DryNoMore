#include "power_ctrl.hpp"
#include "config.hpp"

void ShiftReg::init() const {
  pinMode(SHIFT_REG_OUTPUT_EN_PIN, OUTPUT);
  disableOutput();

  pinMode(SHIFT_REG_DATA_CLK_PIN, OUTPUT);
  digitalWrite(SHIFT_REG_DATA_CLK_PIN, LOW);

  pinMode(SHIFT_REG_OUTPUT_UPDATE_PIN, OUTPUT);
  digitalWrite(SHIFT_REG_OUTPUT_UPDATE_PIN, LOW);

  pinMode(SHIFT_REG_DATA_PIN, OUTPUT);
  digitalWrite(SHIFT_REG_DATA_PIN, LOW);
}
void ShiftReg::enableOutput() const { digitalWrite(SHIFT_REG_OUTPUT_EN_PIN, LOW); }
void ShiftReg::disableOutput() const { digitalWrite(SHIFT_REG_OUTPUT_EN_PIN, HIGH); }
void ShiftReg::update(uint16_t newValue) const {
  // check my assumptions
  static_assert(LOW == 0x00);
  static_assert(HIGH == 0x01);

  for (uint8_t i = 0; i < 16; ++i) {
    uint8_t targetVal = newValue & 1;
    // first write the serial data
    digitalWrite(SHIFT_REG_DATA_PIN, targetVal);
    // then issue a rising clock edge
    digitalWrite(SHIFT_REG_DATA_CLK_PIN, HIGH);
    digitalWrite(SHIFT_REG_DATA_CLK_PIN, LOW);

    newValue >>= 1;
  }

  // finally update the outputs, again with a rising edge!
  digitalWrite(SHIFT_REG_OUTPUT_UPDATE_PIN, HIGH);
  digitalWrite(SHIFT_REG_OUTPUT_UPDATE_PIN, LOW);
}
