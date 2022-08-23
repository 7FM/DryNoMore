#include "config.hpp"

#include "adc_measurement.hpp"
#include "lan.hpp"
#include "macros.hpp"
#include "power_ctrl.hpp"
#include "serial.hpp"
#include "settings.hpp"
#include "sleep.hpp"

#include <Arduino.h>

// Consts
static constexpr decltype(A0) moistSensPins[] = MOISTURE_SENSOR_PINS;
static constexpr decltype(A0) waterSensPins[] = WATER_SENSOR_PINS;
static constexpr decltype(A0) unusedDigitalPins[] = FREE_DIGITAL_PINS;

static constexpr uint16_t moistSensPwrMap[] = MOIST_SENS_PWR_MAPPING;
static constexpr uint16_t waterSensPwrMap[] = WATER_SENS_PWR_MAPPING;
static constexpr uint16_t pumpPwrMap[] = PUMP_PWR_MAPPING;

// Global vars
static ShiftReg shiftReg;
static Settings settings;
static Status status;

static inline bool isSoilTooDry(uint8_t pin, uint16_t min, uint16_t max,
                                uint8_t target, uint8_t &measurement) {
  SERIALprintP(PSTR("Measured soil moisture"));
  measurement = clampedMeasurement(pin, min, max);
  return measurement < target;
}

static inline bool waterTankNotEmpty(uint8_t pin, uint16_t min, uint16_t max,
                                     uint8_t empty, uint8_t &measurement) {
  SERIALprintP(PSTR("Measured water tank level"));
  measurement = clampedMeasurement(pin, min, max);
  bool isEmpty = measurement <= empty;
  return !isEmpty;
}

static uint8_t checkMoisture(uint8_t idx, Status &status) {
  const auto pumpMask = pumpPwrMap[idx];
  const auto moistPin = moistSensPins[idx];
  const auto moistSensMask = moistSensPwrMap[idx];

  const auto moistMin = settings.sensConfs[idx].minValue;
  const auto moistMax = settings.sensConfs[idx].maxValue;
  const auto moistTarget = settings.targetMoistures[idx];

  const uint8_t waterSensIdx = (settings.moistSensToWaterSensBitmap[idx / 8] >>
                                (idx & 7 /*aka mod 8*/)) &
                               0x01;

  const auto waterPin = waterSensPins[waterSensIdx];
  const auto waterSensMask = waterSensPwrMap[waterSensIdx];

  const uint8_t offsetIdx = (MAX_MOISTURE_SENSOR_COUNT) + waterSensIdx;
  const auto waterMin = settings.sensConfs[offsetIdx].minValue;
  const auto waterMax = settings.sensConfs[offsetIdx].maxValue;
  const auto waterWarning = settings.waterLvlThres[waterSensIdx].warnThres;
  const auto waterEmpty = settings.waterLvlThres[waterSensIdx].emptyThres;

  shiftReg.update(moistSensMask | waterSensMask);
  _delay_ms(POWER_ON_DELAY_MS);
  bool isPumpActive = false;

  // timeout to ensure we are not stuck here forever (and flood the plants) in
  // case of an defect sensor/pump
  constexpr long measurementDuration =
      (static_cast<long>(ADC_MEASUREMENTS) * (MEASURE_DELAY_MS));
  constexpr uint8_t timeoutCycles =
      (static_cast<long>(IRRIGATION_TIMEOUT_SEC) * 1000 + measurementDuration) /
      measurementDuration;

  bool hasWaterLeft = true;
  uint8_t waterMeasurement = UNDEFINED_LEVEL;
  uint8_t moistMeasurement = UNDEFINED_LEVEL;

  auto initCheck = [&]() {
    if (status.beforeWaterLevels[waterSensIdx] == UNDEFINED_LEVEL) {
      status.beforeWaterLevels[waterSensIdx] = waterMeasurement;
    }
    if (status.beforeMoistureLevels[idx] == UNDEFINED_LEVEL) {
      status.beforeMoistureLevels[idx] = moistMeasurement;
    }
  };

  for (uint8_t cycle = 0;
       (hasWaterLeft = waterTankNotEmpty(waterPin, waterMin, waterMax,
                                         waterEmpty, waterMeasurement)) &&
       isSoilTooDry(moistPin, moistMin, moistMax, moistTarget,
                    moistMeasurement);
       ++cycle) {
    if (cycle == 0) {
      initCheck();
    } else if (cycle == timeoutCycles) {
      settings.hardwareFailure = true;
      shiftReg.disableOutput();

      // Send HW error message!
      powerUpEthernet();
      sendErrorHardware(idx);
      powerDownEthernet();
      break;
    }
    if (!isPumpActive) {
      // turn on the pump
      isPumpActive = true;
      shiftReg.update(moistSensMask | waterSensMask | pumpMask);
    }
  }

  // Finally turn the power of the sensors and the pump off
  shiftReg.disableOutput();
  shiftReg.update(0);
  shiftReg.enableOutput();

  initCheck();
  status.afterWaterLevels[waterSensIdx] = waterMeasurement;
  status.afterMoistureLevels[idx] = moistMeasurement;

  uint8_t retCode =
      (hasWaterLeft ? 0 : 0x02) | (waterMeasurement <= waterWarning ? 0x04 : 0);
  return retCode << (waterSensIdx << 1);
}

static void disableDigitalOnAnalogPins() {
  // disable digital input buffer on analog pins, as not used and saves energy
  DIDR0 = _BV(ADC0D) | _BV(ADC1D) | _BV(ADC2D) | _BV(ADC3D) | _BV(ADC4D) |
          _BV(ADC5D);
  DIDR1 = _BV(AIN0D) | _BV(AIN1D);
}

static void powerSavingSettings() {
  // reduce the clock speed for a lower power consumption
  noInterrupts();
  // To avoid unintentional changes of clock frequency, a special write
  // procedure must be followed to change the CLKPS bits:
  // 1. Write the clock prescaler change enable (CLKPCE) bit to one and all
  // other bits in CLKPR to zero.
  CLKPR = _BV(CLKPCE);
  // 2. Within four cycles, write the desired value to CLKPS while writing a
  // zero to CLKPCE. division factor of 256!!! We are getting really really
  // slow... 16 MHz / 256 = 62.5 kHz
  CLKPR = _BV(CLKPS3);
  interrupts();

  // Turn off all unused modules:
  // NOTE that you can not use delay(), millis(), etc. afterwards!
  // Disable all timer interrupts:
  TIMSK0 = 0; /*Disable Timer0 interrupts*/
  TIMSK1 = 0; /*Disable Timer1 interrupts*/
  TIMSK2 = 0; /*Disable Timer2 interrupts*/
  // Power down modules:
  PRR = _BV(PRTWI) /*Shutdown TWI*/ | _BV(PRTIM2) /*Shutdown Timer2*/ |
        _BV(PRTIM1) /*Shutdown Timer1*/ | _BV(PRTIM0) /*Shutdown Timer0*/
#ifdef DISABLE_SPI
        | _BV(PRSPI) /*Shutdown SPI*/
#endif
#ifdef DISABLE_SERIAL
        | _BV(PRUSART0) /*Shutdown USART, needed for Hardware Serial*/
#endif
      ;

  disableDigitalOnAnalogPins();
}

static void setStatusUndef(Status &status) {
  for (uint8_t i = 0; i < (MAX_MOISTURE_SENSOR_COUNT); ++i) {
    status.beforeMoistureLevels[i] = UNDEFINED_LEVEL;
    status.afterMoistureLevels[i] = UNDEFINED_LEVEL;
  }
  for (uint8_t i = 0; i < 2; ++i) {
    status.beforeWaterLevels[i] = UNDEFINED_LEVEL;
    status.afterWaterLevels[i] = UNDEFINED_LEVEL;
  }
}

static void defaultInitStatus(Status &status) {
  for (auto &t : status.ticksSinceIrrigation) {
    t = 255;
  }
}

void setup() {
  // Sanity checks
  static_assert(CONST_ARRAY_SIZE(moistSensPins) ==
                CONST_ARRAY_SIZE(moistSensPwrMap));
  static_assert(CONST_ARRAY_SIZE(moistSensPins) == (MAX_MOISTURE_SENSOR_COUNT));
  static_assert(CONST_ARRAY_SIZE(waterSensPins) ==
                CONST_ARRAY_SIZE(waterSensPwrMap));
  static_assert(CONST_ARRAY_SIZE(waterSensPins) == 2);
  static_assert((MAX_MOISTURE_SENSOR_COUNT) == CONST_ARRAY_SIZE(pumpPwrMap));

  powerSavingSettings();
  setupEthernet();

  SERIALbegin(SERIAL_BAUD_RATE);
  defaultInitSettings(settings);
  defaultInitStatus(status);

  // TODO update!
  for (uint8_t i = 0; i < settings.numPlants; ++i) {
    // TODO rather use INPUT_PULLUP / OUTPUT and change right before turning on
    // the power of the sensor???
    pinMode(moistSensPins[i], INPUT);
  }
  for (uint8_t i = settings.numPlants; i < CONST_ARRAY_SIZE(moistSensPins);
       ++i) {
    // initialize all unused pins, setting this for used pins too results in
    // invalid readings
    // TODO prefer pullup input or rather output?
    pinMode(moistSensPins[i], INPUT_PULLUP);
  }
  const uint8_t usedWaterSens = getUsedWaterSens(settings);
  for (uint8_t i = 0; i < usedWaterSens; ++i) {
    // TODO rather use INPUT_PULLUP / OUTPUT and change right before turning on
    // the power of the sensor???
    pinMode(waterSensPins[i], INPUT);
  }
  for (uint8_t i = usedWaterSens; i < CONST_ARRAY_SIZE(waterSensPins); ++i) {
    // initialize all unused pins, setting this for used pins too results in
    // invalid readings
    // TODO prefer pullup input or rather output?
    pinMode(waterSensPins[i], INPUT_PULLUP);
  }
  // Heavily discussed in
  // https://arduino.stackexchange.com/questions/88319/power-saving-configuration-of-unconnected-pins
  // TLDR; either INPUT_PULLUP or OUTPUT LOW where INPUT_PULLUP seems to suffer
  // a bit from leak current
  for (auto p : unusedDigitalPins) {
    pinMode(p, OUTPUT);
    digitalWrite(p, LOW);
  }
}

void loop() {

  // ===================================================================
  // Setup modes
  // ===================================================================
#ifdef DUMP_SOIL_MOISTURES_MEASUREMENTS
  for (uint8_t idx = 0; idx < settings.numPlants; ++idx) {
    const auto moistPin = moistSensPins[idx];
    const auto moistSensMask = moistSensPwrMap[idx];

    const auto moistMin = settings.sensConfs[idx].minValue;
    const auto moistMax = settings.sensConfs[idx].maxValue;
    const auto moistTarget = settings.targetMoistures[idx];

    shiftReg.update(moistSensMask);
    shiftReg.enableOutput();
    _delay_ms(POWER_ON_DELAY_MS);

    uint8_t measurement;
    isSoilTooDry(moistPin, moistMin, moistMax, moistTarget, measurement);

    // Finally turn the power of the sensors and the pump off
    shiftReg.disableOutput();
  }
#elif defined(DUMP_WATER_LEVEL_MEASUREMENTS)
  const uint8_t usedWaterSens = getUsedWaterSens(settings);
  for (uint8_t idx = 0; idx < usedWaterSens; ++idx) {
    const auto waterPin = waterSensPins[idx];
    const auto waterSensMask = waterSensPwrMap[idx];

    const uint8_t offsetIdx = (MAX_MOISTURE_SENSOR_COUNT) + idx;
    const auto waterMin = settings.sensConfs[offsetIdx].minValue;
    const auto waterMax = settings.sensConfs[offsetIdx].maxValue;
    // const auto waterWarning = settings.waterLvlThres[idx].warnThres;
    const auto waterEmpty = settings.waterLvlThres[idx].emptyThres;

    shiftReg.update(waterSensMask);
    shiftReg.enableOutput();
    _delay_ms(POWER_ON_DELAY_MS);

    uint8_t measurement;
    waterTankNotEmpty(waterPin, waterMin, waterMax, waterEmpty, measurement);

    // Finally turn the power of the sensors and the pump off
    shiftReg.disableOutput();
  }
#else
  // ===================================================================
  // Production mode
  // ===================================================================

  // Check all plants!
  // allow remote to reset the hardware failure flag
  powerUpEthernet();
  updateSettings(settings);
  powerDownEthernet();
  if (!settings.hardwareFailure) {
    bool statusChanged = false;
    setStatusUndef(status);
    status.numPlants = settings.numPlants;
    status.numWaterSensors = getUsedWaterSens(settings);

    // 2 bits per water level sensor: 1 to request a warning, 1 to send an error
    // that the water is empty! the lsb bits are used for the first sensor.
    uint8_t resCode = 0;

    shiftReg.update(0);
    shiftReg.enableOutput();

    for (uint8_t idx = 0; !settings.hardwareFailure && idx < settings.numPlants;
         ++idx) {
      bool skip = status.ticksSinceIrrigation[idx] <
                      settings.ticksBetweenIrrigation[idx] ||
                  ((settings.skipBitmap[idx / 8] >> (idx & 7 /*aka mod 8*/)) &
                   0x01) != 0;
      statusChanged |= !skip;
      if (!skip) {
        // This ensures that the tick counter will overflow to 0 -> reset its
        // value!
        status.ticksSinceIrrigation[idx] = 255;
        resCode |= checkMoisture(idx, status);
      }
    }

    shiftReg.disableOutput();
    shiftReg.update(0);

    // Update the tick counters!
    for (auto &t : status.ticksSinceIrrigation) {
      ++t;
    }

    // Only send messages if the status changed!
    if (statusChanged) {
      powerUpEthernet();
      for (uint8_t i = 0; resCode != 0 && i < 2; ++i, resCode >>= 2) {
        if (resCode & 0x02) {
          sendErrorWaterEmpty(i);
        } else if (resCode & 0x01) {
          sendWarning(i);
        }
      }
      sendStatus(status);
      powerDownEthernet();
    }
  } else {
    // Update the tick counters!
    for (auto &t : status.ticksSinceIrrigation) {
      // prevent overflows!
      t = max(t, t + 1);
    }
  }
  longSleep<SLEEP_PERIOD_MIN>();
#endif
}
