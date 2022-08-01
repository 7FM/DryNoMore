#include "settings.hpp"
#include "config.hpp"
#include "macros.hpp"

void defaultInitSettings(Settings &settings) {
    static constexpr uint16_t moistSensMin[] = DEFAULT_MOISTURE_MIN_VALUES;
    static constexpr uint16_t moistSensMax[] = DEFAULT_MOISTURE_MAX_VALUES;
    static constexpr uint8_t moistureTarget[] = DEFAULT_MOISTURE_TARGET_THRESHOLDS;
    static constexpr uint16_t waterSensMin[] = DEFAULT_WATER_MIN_VALUES;
    static constexpr uint16_t waterSensMax[] = DEFAULT_WATER_MAX_VALUES;
    static constexpr uint8_t waterWarningThres[] = DEFAULT_WATER_WARNING_THRESHOLDS;
    static constexpr uint8_t waterEmptyThres[] = DEFAULT_WATER_EMPTY_THRESHOLDS;

    static_assert(CONST_ARRAY_SIZE(moistSensMin) == CONST_ARRAY_SIZE(moistSensMax));
    static_assert(CONST_ARRAY_SIZE(moistSensMin) == CONST_ARRAY_SIZE(moistureTarget));
    static_assert(CONST_ARRAY_SIZE(moistSensMin) == (MAX_MOISTURE_SENSOR_COUNT));

    static_assert(CONST_ARRAY_SIZE(waterSensMin) == CONST_ARRAY_SIZE(waterSensMax));
    static_assert(CONST_ARRAY_SIZE(waterSensMin) == CONST_ARRAY_SIZE(waterWarningThres));
    static_assert(CONST_ARRAY_SIZE(waterSensMin) == CONST_ARRAY_SIZE(waterEmptyThres));
    static_assert(CONST_ARRAY_SIZE(waterSensMin) == 2);

    for (uint8_t i = 0; i < (MAX_MOISTURE_SENSOR_COUNT); ++i) {
        settings.sensConfs[i].minValue = moistSensMin[i];
        settings.sensConfs[i].maxValue = moistSensMax[i];
        settings.targetMoistures[i] = moistureTarget[i];
    }
    for (uint8_t i = 0; i < 2; ++i) {
        settings.sensConfs[(MAX_MOISTURE_SENSOR_COUNT) + i].minValue = waterSensMin[i];
        settings.sensConfs[(MAX_MOISTURE_SENSOR_COUNT) + i].maxValue = waterSensMax[i];
        settings.waterLvlThres[i].warnThres = waterWarningThres[i];
        settings.waterLvlThres[i].emptyThres = waterEmptyThres[i];
    }
    for (uint8_t i = 0; i < CONST_ARRAY_SIZE(settings.moistSensToWaterSensBitmap); ++i) {
        // By default map only use water sensor 1
        settings.moistSensToWaterSensBitmap[i] = 0;
    }
    settings.numPlants = DEFAULT_NUM_PLANTS;
}

uint8_t getUsedWaterSens(const Settings &settings) {
    uint8_t usedWaterSens;
    for (uint8_t bm : settings.moistSensToWaterSensBitmap) {
        usedWaterSens |= bm;
    }
    return usedWaterSens != 0 ? 2 : 1;
}
