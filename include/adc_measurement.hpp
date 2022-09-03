#pragma once

#include <Arduino.h>

uint16_t adcMeasurement(uint8_t pin);
uint8_t clampedMeasurement(uint8_t pin, uint16_t min, uint16_t max,
                           uint16_t &rawMeasurement);
