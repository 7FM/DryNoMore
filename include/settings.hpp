#pragma once

#include <Arduino.h>

#define MAX_MOISTURE_SENSOR_COUNT 6

struct SensConfig {
    uint16_t minValue;
    uint16_t maxValue;
};
struct WaterLvlThresholds {
    uint8_t warnThres;
    uint8_t emptyThres;
};


// TODO we could use the EEPROM for the following parameters!
struct Settings {
    SensConfig sensConfs[(MAX_MOISTURE_SENSOR_COUNT) + 2];
    WaterLvlThresholds waterLvlThres[2];
    uint8_t targetMoistures[MAX_MOISTURE_SENSOR_COUNT];
    uint8_t moistSensToWaterSensBitmap[((MAX_MOISTURE_SENSOR_COUNT) + 8 - 1) / 8];
    uint8_t numPlants;
};

void defaultInitSettings(Settings &settings);
uint8_t getUsedWaterSens(const Settings &settings);
