#pragma once

#include <Arduino.h>

#include "settings_defs.hpp"

void defaultInitSettings(Settings &settings);
uint8_t getUsedWaterSens(const Settings &settings);
