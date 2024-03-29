#include "settings.hpp"
#include "config.hpp"
#include "macros.hpp"

void defaultInitSettings(Settings &settings) {
  static constexpr uint16_t moistSensMin[] = DEFAULT_MOISTURE_MIN_VALUES;
  static constexpr uint16_t moistSensMax[] = DEFAULT_MOISTURE_MAX_VALUES;
  static constexpr uint8_t moistureTarget[] =
      DEFAULT_MOISTURE_TARGET_THRESHOLDS;
  static constexpr uint8_t burstDuration[] = DEFAULT_BURST_DURATION_SEC;
  static constexpr uint8_t burstDelay[] = DEFAULT_BURST_DELAY_SEC;
  static constexpr uint8_t maxBursts[] = DEFAULT_MAX_BURSTS;
  static constexpr bool moistureToWaterMappings[] =
      DEFAULT_MOIST_TO_WATER_MAPPINGS;
  static constexpr bool skipPlant[] = DEFAULT_PLANT_SKIP_VALUES;

  static constexpr uint16_t waterSensMin[] = DEFAULT_WATER_MIN_VALUES;
  static constexpr uint16_t waterSensMax[] = DEFAULT_WATER_MAX_VALUES;
  static constexpr uint8_t waterWarningThres[] =
      DEFAULT_WATER_WARNING_THRESHOLDS;
  static constexpr uint8_t waterEmptyThres[] = DEFAULT_WATER_EMPTY_THRESHOLDS;

  static_assert(CONST_ARRAY_SIZE(moistSensMin) ==
                CONST_ARRAY_SIZE(moistSensMax));
  static_assert(CONST_ARRAY_SIZE(moistSensMin) ==
                CONST_ARRAY_SIZE(moistureTarget));
  static_assert(CONST_ARRAY_SIZE(moistureToWaterMappings) ==
                CONST_ARRAY_SIZE(moistureTarget));
  static_assert(CONST_ARRAY_SIZE(burstDuration) ==
                CONST_ARRAY_SIZE(moistureTarget));
  static_assert(CONST_ARRAY_SIZE(burstDelay) ==
                CONST_ARRAY_SIZE(moistureTarget));
  static_assert(CONST_ARRAY_SIZE(maxBursts) ==
                CONST_ARRAY_SIZE(moistureTarget));
  static_assert(CONST_ARRAY_SIZE(skipPlant) ==
                CONST_ARRAY_SIZE(moistureTarget));
  static_assert(CONST_ARRAY_SIZE(moistSensMin) == (MAX_MOISTURE_SENSOR_COUNT));

  static_assert(CONST_ARRAY_SIZE(waterSensMin) ==
                CONST_ARRAY_SIZE(waterSensMax));
  static_assert(CONST_ARRAY_SIZE(waterSensMin) ==
                CONST_ARRAY_SIZE(waterWarningThres));
  static_assert(CONST_ARRAY_SIZE(waterSensMin) ==
                CONST_ARRAY_SIZE(waterEmptyThres));
  static_assert(CONST_ARRAY_SIZE(waterSensMin) == 2);

  for (uint8_t i = 0; i < (MAX_MOISTURE_SENSOR_COUNT); ++i) {
    settings.sensConfs[i].minValue = moistSensMin[i];
    settings.sensConfs[i].maxValue = moistSensMax[i];
    settings.targetMoisture[i] = moistureTarget[i];
    settings.burstDuration[i] = burstDuration[i];
    settings.burstDelay[i] = burstDelay[i];
    settings.maxBursts[i] = maxBursts[i];
    settings.ticksBetweenIrrigation[i] = 0;
  }
  for (uint8_t i = 0; i < 2; ++i) {
    settings.sensConfs[(MAX_MOISTURE_SENSOR_COUNT) + i].minValue =
        waterSensMin[i];
    settings.sensConfs[(MAX_MOISTURE_SENSOR_COUNT) + i].maxValue =
        waterSensMax[i];
    settings.waterLvlThres[i].warnThres = waterWarningThres[i];
    settings.waterLvlThres[i].emptyThres = waterEmptyThres[i];
  }
  for (uint8_t i = 0; i < CONST_ARRAY_SIZE(settings.moistSensToWaterSensBitmap);
       ++i) {
    // zero init
    settings.moistSensToWaterSensBitmap[i] = 0;
    settings.skipBitmap[i] = 0;
  }
  for (uint8_t i = 0; i < CONST_ARRAY_SIZE(moistureToWaterMappings); ++i) {
    // apply defaults
    settings.moistSensToWaterSensBitmap[i / 8] |=
        ((moistureToWaterMappings[i] ? 1 : 0) << (i & 7 /* aka mod 8 */));
    settings.skipBitmap[i / 8] |=
        ((skipPlant[i] ? 1 : 0) << (i & 7 /* aka mod 8 */));
  }
  settings.numPlants = DEFAULT_NUM_PLANTS;
  settings.hardwareFailure = false;
  settings.debug = true;
}

uint8_t getUsedWaterSens(const Settings &settings) {
  uint8_t usedWaterSens = 0;
  for (uint8_t bm : settings.moistSensToWaterSensBitmap) {
    usedWaterSens |= bm;
  }
  return usedWaterSens != 0 ? 2 : 1;
}
