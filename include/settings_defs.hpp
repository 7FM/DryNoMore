#pragma once

#include "packed.hpp"

#define MAX_MOISTURE_SENSOR_COUNT 6

PACKED_STRUCT_DEF(SensConfig, uint16_t minValue; uint16_t maxValue;);
PACKED_STRUCT_DEF(WaterLvlThresholds, uint8_t warnThres; uint8_t emptyThres;);

PACKED_STRUCT_DEF(
    Settings, SensConfig sensConfs[(MAX_MOISTURE_SENSOR_COUNT) + 2];
    WaterLvlThresholds waterLvlThres[2];
    uint8_t targetMoistures[MAX_MOISTURE_SENSOR_COUNT]; uint8_t
        moistSensToWaterSensBitmap[((MAX_MOISTURE_SENSOR_COUNT) + 8 - 1) / 8];
    uint8_t skipBitmap[((MAX_MOISTURE_SENSOR_COUNT) + 8 - 1) / 8];
    uint8_t numPlants;);
