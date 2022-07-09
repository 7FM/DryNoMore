#pragma once

// Config
#define MEASURE_DELAY_MS 200
#define POWER_ON_DELAY_MS 500
#define ADC_MEASUREMENTS 5
#define IRRIGATION_TIMEOUT_SEC 20

// time between moisture checks: default every 6 hours
#define SLEEP_PERIOD_MIN 6 * 60

// TODO we could use the EEPROM for most of these parameters!

// TODO
#define MOISTURE_SENSOR_PINS \
    { A7 }
// TODO
#define MOISTURE_SENSOR_POWER_PINS \
    { 0 }
// TODO
#define MOSFET_PINS \
    { 1 }

#define MOISTURE_MIN_VALUES \
    { 200 } // TODO
#define MOISTURE_MAX_VALUES \
    { 500 } // TODO
#define MOISTURE_TARGETS \
    { 50 } // TODO
