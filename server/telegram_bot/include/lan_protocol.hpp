#pragma once

#include "packed.hpp"
#include "settings_defs.hpp"

#define UNDEFINED_LEVEL_16 0xFFFF
#define UNDEFINED_LEVEL_8 0xFF

PACKED_STRUCT_DEF(Status,
                  uint8_t ticksSinceIrrigation[MAX_MOISTURE_SENSOR_COUNT];
                  uint8_t beforeMoistureLevels[MAX_MOISTURE_SENSOR_COUNT];
                  uint8_t afterMoistureLevels[MAX_MOISTURE_SENSOR_COUNT];
                  uint8_t beforeWaterLevels[2]; uint8_t afterWaterLevels[2];
                  uint16_t beforeMoistureLevelsRaw[MAX_MOISTURE_SENSOR_COUNT];
                  uint16_t afterMoistureLevelsRaw[MAX_MOISTURE_SENSOR_COUNT];
                  uint16_t beforeWaterLevelsRaw[2];
                  uint16_t afterWaterLevelsRaw[2]; uint8_t numPlants;
                  uint8_t numWaterSensors;);

enum PacketType : uint8_t {
  INFO_MSG = 1,
  WARN_MSG = 2,
  ERR_MSG = 4,
  FAILURE_MSG = 8,
  REPORT_STATUS = 16,
  REQUEST_SETTINGS = 32
};
