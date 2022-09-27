#pragma once

#include <Arduino.h>

#include "config.hpp"
#include "lan_protocol.hpp"

#ifdef USE_ETHERNET
#include "power_ctrl.hpp"

#ifdef ETH_PWR_MAPPING
void setupEthernet(const ShiftReg &shiftReg);
void powerUpEthernet(const ShiftReg &shiftReg);
void powerDownEthernet(const ShiftReg &shiftReg);
#else
void powerUpEthernet(const ShiftReg &shiftReg) { powerUpEthernet(); }
void powerUpEthernet();
void powerDownEthernet(const ShiftReg &shiftReg) { powerDownEthernet(); }
void powerDownEthernet();
void setupEthernet(const ShiftReg &shiftReg) { setupEthernet(); }
void setupEthernet();
#endif

void sendStatus(const Status &status);
void sendWarning(uint8_t waterSensIdx);

// Forward declaration
class Settings;
void updateSettings(Settings &settings);

void sendErrorWaterEmpty(uint8_t waterSensIdx);
void sendErrorHardware(uint8_t moistSensIdx);
#else
#define setupEthernet(...)
#define powerUpEthernet(...)
#define powerDownEthernet(...)
#define sendStatus(...)
#define sendWarning(...)
#define updateSettings(...)
#define sendErrorWaterEmpty(...)
#define sendErrorHardware(...)
#endif
