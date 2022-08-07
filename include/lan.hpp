#pragma once

#include <Arduino.h>

#include "config.hpp"
#include "lan_protocol.hpp"

#ifdef USE_ETHERNET

void initEthernet();
void deinitEthernet();

// TODO
void sendStatus(const Status &status);
void sendWarning(uint8_t waterSensIdx);

// Forward declaration
class Settings;
void updateSettings(Settings &settings);

// TODO unify
void sendErrorWaterEmpty(uint8_t waterSensIdx);
void sendErrorHardware(uint8_t moistSensIdx, uint8_t waterSensIdx);
#else
#define initEthernet(...)
#define deinitEthernet(...)
#define sendStatus(...)
#define sendWarning(...)
#define updateSettings(...)
#define sendErrorWaterEmpty(...)
#define sendErrorHardware(...)
#endif
