#pragma once

// Configure modes
// #define DUMP_SOIL_MOISTURES_MEASUREMENTS
#ifndef DUMP_SOIL_MOISTURES_MEASUREMENTS
#define DUMP_WATER_LEVEL_MEASUREMENTS
#endif

#if !defined(DUMP_SOIL_MOISTURES_MEASUREMENTS) && !defined(DUMP_WATER_LEVEL_MEASUREMENTS)
// No serial needed 
#define DISABLE_SERIAL
#else
#warning "Running in setup mode, this won't irrigate your flowers!"
#warning "Disable both DUMP_SOIL_MOISTURES_MEASUREMENTS and DUMP_WATER_LEVEL_MEASUREMENTS in include/config.hpp to run in production mode"
#endif

// Config
#define MEASURE_DELAY_MS 200
#define POWER_ON_DELAY_MS 500
#define ADC_MEASUREMENTS 5
#define IRRIGATION_TIMEOUT_SEC 20

// time between moisture checks: default every 6 hours
#define SLEEP_PERIOD_MIN 6 * 60

// TODO we could use the EEPROM for most of these parameters!

#define SHIFT_REG_DATA_CLK_PIN 6
#define SHIFT_REG_OUTPUT_UPDATE_PIN 7
#define SHIFT_REG_OUTPUT_EN_PIN 8
#define SHIFT_REG_DATA_PIN 9

#define MOISTURE_SENSOR_PINS \
    { A2, A3, A7, A4, A5, A6 }

#define WATER_SENSOR_PINS \
    { A0, A1 }

// uint16_t shiftReg: P = Pump, M = moisture sensor, W = water sensor
// { UNUSED_1, P1, P6, P5, P4, P3, P2, UNUSED_2, M6, W1, M3, M2, M1, M5, M4, W2 }
#define PUMP_PWR_MAPPING                              \
    {                                                 \
        (static_cast<uint16_t>(1) << 1), /* Pump 1 */ \
        (static_cast<uint16_t>(1) << 6), /* Pump 2 */ \
        (static_cast<uint16_t>(1) << 5), /* Pump 3 */ \
        (static_cast<uint16_t>(1) << 4), /* Pump 4 */ \
        (static_cast<uint16_t>(1) << 3), /* Pump 5 */ \
        (static_cast<uint16_t>(1) << 2)  /* Pump 6 */ \
    }

#define MOIST_SENS_PWR_MAPPING                         \
    {                                                  \
        (static_cast<uint16_t>(1) << 12), /* Sens 1 */ \
        (static_cast<uint16_t>(1) << 11), /* Sens 2 */ \
        (static_cast<uint16_t>(1) << 10), /* Sens 3 */ \
        (static_cast<uint16_t>(1) << 14), /* Sens 4 */ \
        (static_cast<uint16_t>(1) << 13), /* Sens 5 */ \
        (static_cast<uint16_t>(1) << 8)   /* Sens 6 */ \
    }

#define WATER_SENS_PWR_MAPPING                        \
    {                                                 \
        (static_cast<uint16_t>(1) << 9), /* Sens 1 */ \
        (static_cast<uint16_t>(1) << 15) /* Sens 2 */ \
    }

#define UNUSED_PWR_MAPPING                              \
    {                                                   \
        (static_cast<uint16_t>(1) << 0), /* Unused 1 */ \
        (static_cast<uint16_t>(1) << 7)  /* Unused 2 */ \
    }

#define MOISTURE_MIN_VALUES \
    { 200 } // TODO
#define MOISTURE_MAX_VALUES \
    { 500 } // TODO
#define MOISTURE_TARGET_THRESHOLDS \
    { 50 } // TODO

#define WATER_MIN_VALUES \
    { 200 } // TODO
#define WATER_MAX_VALUES \
    { 500 } // TODO
#define WATER_WARNING_THRESHOLDS \
    { 50 } // TODO
#define WATER_EMPTY_THRESHOLDS \
    { 20 } // TODO
