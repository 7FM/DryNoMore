#pragma once

// #define BOARD_VERSION 0x0010 /*v0.1*/
// #define BOARD_VERSION 0x0020 /*v0.2*/
#define BOARD_VERSION 0x0030 /*v0.3*/

// Configure modes
// #define DUMP_SOIL_MOISTURES_MEASUREMENTS
#ifndef DUMP_SOIL_MOISTURES_MEASUREMENTS
// #define DUMP_WATER_LEVEL_MEASUREMENTS
#endif

// Enable debug serial prints
// #define DEBUG_SERIAL_PRINTS
// #define DEBUG_NORMAL_CPU_SPEED

#if !defined(DUMP_SOIL_MOISTURES_MEASUREMENTS) &&                              \
    !defined(DUMP_WATER_LEVEL_MEASUREMENTS)
// No serial needed
#define DISABLE_SERIAL
#else
#warning "Running in setup mode, this won't irrigate your flowers!"
#warning "Disable both DUMP_SOIL_MOISTURES_MEASUREMENTS and "                       \
    "DUMP_WATER_LEVEL_MEASUREMENTS in include/config.hpp to run in production mode"
#endif

#define USE_ETHERNET
#ifdef USE_ETHERNET

#define MAC_ADDRESS                                                            \
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED }

#define LOCAL_NETWORK_SUBNET 192, 168, 0
#define FALLBACK_IP LOCAL_NETWORK_SUBNET, 254
#define FALLBACK_DNS LOCAL_NETWORK_SUBNET, 1
#define LOCAL_SERVER_IP LOCAL_NETWORK_SUBNET, 42
#define LOCAL_SERVER_PORT 42424
#else
// SPI will be required for the Ethernet connection
#define DISABLE_SPI
#endif

// Config

// We can't increase the serial speed much with our low CPU frequency!
#ifdef DEBUG_NORMAL_CPU_SPEED
#define SERIAL_BAUD_RATE 9600
#else
#define SERIAL_BAUD_RATE 600
#endif

#define MEASURE_DELAY_MS 200
#define POWER_ON_DELAY_MS 500
#define ADC_MEASUREMENTS 5

// time between moisture checks: default every 6 hours
#define SLEEP_PERIOD_MIN static_cast<uint16_t>(6) * 60

#define SHIFT_REG_DATA_CLK_PIN 6
#define SHIFT_REG_OUTPUT_UPDATE_PIN 7
#define SHIFT_REG_OUTPUT_EN_PIN 8
#define SHIFT_REG_DATA_PIN 9

#define MOISTURE_SENSOR_PINS                                                   \
  { A2, A3, A7, A4, A5, A6 }

#define WATER_SENSOR_PINS                                                      \
  { A0, A1 }

#ifdef DEBUG_SERIAL_PRINTS
// Reactivate the serial port if we desire serial prints!
#ifdef DISABLE_SERIAL
#undef DISABLE_SERIAL
#endif
#endif

#ifdef DISABLE_SERIAL
#define FREE_DIGITAL_PINS                                                      \
  { /*D*/ 0, /*D*/ 1, /*D*/ 2, /*D*/ 3, /*D*/ 4, /*D*/ 5 }
#else
#define FREE_DIGITAL_PINS                                                      \
  { /*D*/ 2, /*D*/ 3, /*D*/ 4, /*D*/ 5 }
#endif

// uint16_t shiftReg: P = Pump, M = moisture sensor, W = water sensor
// { UNUSED_1, P1, P6, P5, P4, P3, P2, UNUSED_2, M6, W1, M3, M2, M1, M5, M4, W2
// }
//   ^--- MSb                                                         LSb ---^
#define PUMP_PWR_MAPPING                                                       \
  {                                                                            \
    (static_cast<uint16_t>(1) << 14),     /* Pump 1 */                         \
        (static_cast<uint16_t>(1) << 9),  /* Pump 2 */                         \
        (static_cast<uint16_t>(1) << 10), /* Pump 3 */                         \
        (static_cast<uint16_t>(1) << 11), /* Pump 4 */                         \
        (static_cast<uint16_t>(1) << 12), /* Pump 5 */                         \
        (static_cast<uint16_t>(1) << 13)  /* Pump 6 */                         \
  }

#define MOIST_SENS_PWR_MAPPING                                                 \
  {                                                                            \
    (static_cast<uint16_t>(1) << 3),     /* Sens 1 */                          \
        (static_cast<uint16_t>(1) << 4), /* Sens 2 */                          \
        (static_cast<uint16_t>(1) << 5), /* Sens 3 */                          \
        (static_cast<uint16_t>(1) << 1), /* Sens 4 */                          \
        (static_cast<uint16_t>(1) << 2), /* Sens 5 */                          \
        (static_cast<uint16_t>(1) << 7)  /* Sens 6 */                          \
  }

#define WATER_SENS_PWR_MAPPING                                                 \
  {                                                                            \
    (static_cast<uint16_t>(1) << 6),    /* Sens 1 */                           \
        (static_cast<uint16_t>(1) << 0) /* Sens 2 */                           \
  }

#if BOARD_VERSION >= 0x0030
#define ETH_PWR_MAPPING (static_cast<uint16_t>(1) << 8) /* Unused 2 */
#define UNUSED_PWR_MAPPING                                                     \
  { (static_cast<uint16_t>(1) << 15) /* Unused 1 */ }

#else
#define UNUSED_PWR_MAPPING                                                     \
  {                                                                            \
    (static_cast<uint16_t>(1) << 15),   /* Unused 1 */                         \
        (static_cast<uint16_t>(1) << 8) /* Unused 2 */                         \
  }
#endif

#define DEFAULT_NUM_PLANTS 1

#define DEFAULT_MOISTURE_MIN_VALUES                                            \
  { 200, 200, 200, 200, 200, 200 }
#define DEFAULT_MOISTURE_MAX_VALUES                                            \
  { 500, 500, 500, 500, 500, 500 }
#define DEFAULT_MOISTURE_TARGET_THRESHOLDS                                     \
  { 50, 50, 50, 50, 50, 50 }
#define DEFAULT_MOIST_TO_WATER_MAPPINGS                                        \
  { 0, 0, 0, 0, 0, 0 }
#define DEFAULT_PLANT_SKIP_VALUES                                              \
  { 1, 1, 1, 1, 1, 1 }
#define DEFAULT_BURST_DURATION_SEC                                             \
  { 2, 2, 2, 2, 2, 2 }
#define DEFAULT_BURST_DELAY_SEC                                                \
  { 5, 5, 5, 5, 5, 5 }
#define DEFAULT_MAX_BURSTS                                                     \
  { 5, 5, 5, 5, 5, 5 }

#define DEFAULT_WATER_MIN_VALUES                                               \
  { 180, 180 }
#define DEFAULT_WATER_MAX_VALUES                                               \
  { 480, 480 }
#define DEFAULT_WATER_WARNING_THRESHOLDS                                       \
  { 50, 50 }
#define DEFAULT_WATER_EMPTY_THRESHOLDS                                         \
  { 20, 20 }

#ifndef DISABLE_SERIAL
#warning                                                                       \
    "Disabling serial is strongly recommended in production mode to avoid "    \
    "unnecessary runtime as well as power consumption!"
#endif
