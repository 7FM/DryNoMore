#pragma once

#include <Arduino.h>

#include "config.hpp"
#include "lan_protocol.hpp"

#ifdef USE_ETHERNET

void setupEthernet();
void powerUpEthernet();
void powerDownEthernet();

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
