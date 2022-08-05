#pragma once

#include "config.hpp"

#ifdef USE_ETHERNET
void initEthernet();
void deinitEthernet();

// TODO
void sendStatus();
void sendWarning();

// Forward declaration
class Settings;
void updateSettings(Settings &settings);

// TODO unify
void sendErrorWaterEmpty();
void sendErrorHardware();
#else
#define initEthernet(...)
#define deinitEthernet(...)
#define sendStatus(...)
#define sendWarning(...)
#define updateSettings(...)
#define sendErrorWaterEmpty(...)
#define sendErrorHardware(...)
#endif
