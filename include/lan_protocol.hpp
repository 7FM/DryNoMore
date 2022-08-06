#pragma once

#include "packed.hpp"
#include "settings_defs.hpp"

#define UNDEFINED_LEVEL 255

PACKED_STRUCT_DEF(Status,
                  uint8_t beforeMoistureLevels[MAX_MOISTURE_SENSOR_COUNT];
                  uint8_t afterMoistureLevels[MAX_MOISTURE_SENSOR_COUNT];
                  uint8_t beforeWaterLevels[2];
                  uint8_t afterWaterLevels[2];);

enum PacketType : uint8_t {
    INFO_MSG = 1,
    WARN_MSG = 2,
    ERR_MSG = 4,
    REPORT_STATUS = 8,
    REQUEST_SETTINGS = 16
};
