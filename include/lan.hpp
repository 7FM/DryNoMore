#pragma once

#include <Arduino.h>

#include "config.hpp"
#include "lan_protocol.hpp"

#ifdef USE_ETHERNET
#include "power_ctrl.hpp"

void setupEthernet(const ShiftReg &shiftReg);
bool powerUpEthernet(const ShiftReg &shiftReg);
void powerDownEthernet(const ShiftReg &shiftReg);

void sendStatus(const Status &status);
void sendWarning(uint8_t waterSensIdx);

// Forward declaration
class Settings;
void updateSettings(Settings &settings);

void sendErrorWaterEmpty(uint8_t waterSensIdx);
void sendErrorHardware(uint8_t moistSensIdx);
#else
#define setupEthernet(...)
#define powerUpEthernet(...) return false
#define powerDownEthernet(...)
#define sendStatus(...)
#define sendWarning(...)
#define updateSettings(...)
#define sendErrorWaterEmpty(...)
#define sendErrorHardware(...)
#endif
