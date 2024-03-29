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
static const ShiftReg shiftReg;
static Settings settings;
static Status status;

static inline bool isSoilTooDry(uint8_t pin, uint16_t min, uint16_t max,
                                uint8_t target, uint8_t &measurement,
                                uint16_t &rawMeasurement) {
  SERIALprintP(PSTR("Measured soil moisture"));
  measurement = clampedMeasurement(pin, min, max, rawMeasurement);
  return measurement < target;
}

static inline bool waterTankNotEmpty(uint8_t pin, uint16_t min, uint16_t max,
                                     uint8_t empty, uint8_t &measurement,
                                     uint16_t &rawMeasurement) {
  SERIALprintP(PSTR("Measured water tank level"));
  measurement = clampedMeasurement(pin, min, max, rawMeasurement);
  bool isEmpty = measurement <= empty;
  return !isEmpty;
}

static uint8_t checkMoisture(uint8_t idx, Status &status,
                             uint16_t &secondsPassed) {
  const auto pumpMask = pumpPwrMap[idx];
  const auto moistPin = moistSensPins[idx];
  const auto moistSensMask = moistSensPwrMap[idx];

  const auto moistMin = settings.sensConfs[idx].minValue;
  const auto moistMax = settings.sensConfs[idx].maxValue;
  const auto moistTarget = settings.targetMoisture[idx];
  const auto burstDelay = settings.burstDelay[idx];
  const auto maxBursts = settings.maxBursts[idx];

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

  // timeout to ensure we are not stuck here forever (and flood the plants) in
  // case of an defect sensor/pump
  constexpr uint32_t measurementDuration =
      (static_cast<uint32_t>(ADC_MEASUREMENTS) * (MEASURE_DELAY_MS));
  const uint8_t burstCycles =
      (static_cast<uint32_t>(settings.burstDuration[idx]) * 1000 +
       measurementDuration - 1) /
      measurementDuration;

  uint16_t rawWaterMeasurement = UNDEFINED_LEVEL_16;
  uint16_t rawMoistMeasurement = UNDEFINED_LEVEL_16;
  uint8_t waterMeasurement = UNDEFINED_LEVEL_8;
  uint8_t moistMeasurement = UNDEFINED_LEVEL_8;

  auto initCheck = [&]() {
    if (status.beforeWaterLevels[waterSensIdx] == UNDEFINED_LEVEL_8) {
      status.beforeWaterLevels[waterSensIdx] = waterMeasurement;
    }
    if (status.beforeMoistureLevels[idx] == UNDEFINED_LEVEL_8) {
      status.beforeMoistureLevels[idx] = moistMeasurement;
    }
    if (status.beforeWaterLevelsRaw[waterSensIdx] == UNDEFINED_LEVEL_16) {
      status.beforeWaterLevelsRaw[waterSensIdx] = rawWaterMeasurement;
    }
    if (status.beforeMoistureLevelsRaw[idx] == UNDEFINED_LEVEL_16) {
      status.beforeMoistureLevelsRaw[idx] = rawMoistMeasurement;
    }
  };

  bool soilIsTooDry = false;
  bool hasWaterLeft = false;
  uint8_t burst = 0;
  uint8_t burstCycle = 0;
  for (; burst < maxBursts; ++burst) {
    // Check soil moisture once, if it is too low, then irrigate a whole burst!
    // After the wait time, check again. This avoids stopping the irrigation
    // because the sensor is covered with water and the water does not
    // immediately seep into the earth.
    if (!(soilIsTooDry = isSoilTooDry(moistPin, moistMin, moistMax, moistTarget,
                                      moistMeasurement, rawMoistMeasurement))) {
      goto stop_irrigation;
    }

    for (; burstCycle < burstCycles; ++burstCycle) {
      if (!(hasWaterLeft =
                waterTankNotEmpty(waterPin, waterMin, waterMax, waterEmpty,
                                  waterMeasurement, rawWaterMeasurement))) {
        goto stop_irrigation;
      }

      if (burstCycle == 0) {
        shiftReg.update(moistSensMask | waterSensMask | pumpMask);
        if (burst == 0) {
          initCheck();
        }
      }
    }
    burstCycle = 0;

    if (burstDelay > 0) {
      // Turn off the pump and wait approx. X seconds
      shiftReg.update(moistSensMask | waterSensMask);
      secondsPassed += burstDelay;
      sleepSec(burstDelay);
    }
  }

stop_irrigation:
  // Finally turn the power of the sensors and the pump off
  shiftReg.disableOutput();
  shiftReg.update(0);
  shiftReg.enableOutput();

  // also add the measurement delays to the seconds passed!
  secondsPassed += static_cast<uint16_t>(
      (static_cast<uint32_t>(
           burst * (burstCycles +
                    1 /*measuring the soil only once before every burst*/) +
           burstCycle) *
       measurementDuration) /
      1000);
  // Less precise but faster measurement delay calculation
  // secondsPassed += static_cast<uint16_t>(burst * burstCycles + burstCycle) *
  //                  settings.burstDuration[idx];

  settings.hardwareFailure = soilIsTooDry && hasWaterLeft;
  initCheck();
  status.afterWaterLevelsRaw[waterSensIdx] = rawWaterMeasurement;
  status.afterWaterLevels[waterSensIdx] = waterMeasurement;
  status.afterMoistureLevelsRaw[idx] = rawMoistMeasurement;
  status.afterMoistureLevels[idx] = moistMeasurement;

  uint8_t retCode =
      (hasWaterLeft ? 0 : 0x02) | (waterMeasurement <= waterWarning ? 0x01 : 0);
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
#ifndef DEBUG_NORMAL_CPU_SPEED
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
#endif

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
    status.beforeMoistureLevels[i] = UNDEFINED_LEVEL_8;
    status.afterMoistureLevels[i] = UNDEFINED_LEVEL_8;
    status.beforeMoistureLevelsRaw[i] = UNDEFINED_LEVEL_16;
    status.afterMoistureLevelsRaw[i] = UNDEFINED_LEVEL_16;
  }
  for (uint8_t i = 0; i < 2; ++i) {
    status.beforeWaterLevels[i] = UNDEFINED_LEVEL_8;
    status.afterWaterLevels[i] = UNDEFINED_LEVEL_8;
    status.beforeWaterLevelsRaw[i] = UNDEFINED_LEVEL_16;
    status.afterWaterLevelsRaw[i] = UNDEFINED_LEVEL_16;
  }
}

static void defaultInitStatus(Status &status) {
  for (auto &t : status.ticksSinceIrrigation) {
    t = 255;
  }
}

static void analogPowerSave() {
  // TODO we might prefer output LOW for connected sensors as they have an
  // capacitor and resistor connected to this pin
  for (auto p : moistSensPins) {
    // pinMode(p, INPUT_PULLUP);
    pinMode(p, OUTPUT);
    digitalWrite(p, LOW);
  }
  for (auto p : waterSensPins) {
    // pinMode(p, INPUT_PULLUP);
    pinMode(p, OUTPUT);
    digitalWrite(p, LOW);
  }
}

static void deinitUnusedAnalogPins() {
  for (uint8_t i = settings.numPlants; i < CONST_ARRAY_SIZE(moistSensPins);
       ++i) {
    // pinMode(moistSensPins[i], INPUT_PULLUP);
    pinMode(moistSensPins[i], OUTPUT);
    digitalWrite(moistSensPins[i], LOW);
  }
  const uint8_t usedWaterSens = getUsedWaterSens(settings);
  for (uint8_t i = usedWaterSens; i < CONST_ARRAY_SIZE(waterSensPins); ++i) {
    // pinMode(waterSensPins[i], INPUT_PULLUP);
    pinMode(waterSensPins[i], OUTPUT);
    digitalWrite(waterSensPins[i], LOW);
  }
}

static void initAnalogPins() {
  for (uint8_t i = 0; i < CONST_ARRAY_SIZE(moistSensPins); ++i) {
    pinMode(moistSensPins[i], INPUT);
  }
  for (uint8_t i = 0; i < CONST_ARRAY_SIZE(waterSensPins); ++i) {
    pinMode(waterSensPins[i], INPUT);
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
  analogPowerSave();

  // Heavily discussed in
  // https://arduino.stackexchange.com/questions/88319/power-saving-configuration-of-unconnected-pins
  // TLDR; either INPUT_PULLUP or OUTPUT LOW where INPUT_PULLUP seems to suffer
  // a bit from leakage current
  // However if we set the TX & RX pins to LOW then their LEDs will always be
  // on! so lets settle with INPUT_PULLUP and hope that the leakage current is
  // much lower than powering the LEDs!
  for (auto p : unusedDigitalPins) {
    if (p == /*D*/ 0 || p == /*D*/ 1) {
      pinMode(p, INPUT_PULLUP);
    } else {
      pinMode(p, OUTPUT);
      digitalWrite(p, LOW);
    }
  }

  SERIALbegin(SERIAL_BAUD_RATE);
  SERIALprintlnP(PSTR("Setup Ethernet"));
  setupEthernet(shiftReg);
  SERIALprintlnP(PSTR("Done: Setup Ethernet"));

  defaultInitSettings(settings);
  defaultInitStatus(status);
}

void loop() {

  // ===================================================================
  // Setup modes
  // ===================================================================
#ifdef DUMP_SOIL_MOISTURES_MEASUREMENTS
  initAnalogPins();
  deinitUnusedAnalogPins();
  for (uint8_t idx = 0; idx < settings.numPlants; ++idx) {
    const auto moistPin = moistSensPins[idx];
    const auto moistSensMask = moistSensPwrMap[idx];

    const auto moistMin = settings.sensConfs[idx].minValue;
    const auto moistMax = settings.sensConfs[idx].maxValue;
    const auto moistTarget = settings.targetMoisture[idx];

    shiftReg.update(moistSensMask);
    shiftReg.enableOutput();
    _delay_ms(POWER_ON_DELAY_MS);

    uint8_t measurement;
    uint16_t rawMeasurement;
    isSoilTooDry(moistPin, moistMin, moistMax, moistTarget, measurement,
                 rawMeasurement);

    // Finally turn the power of the sensors and the pump off
    shiftReg.disableOutput();
  }
#elif defined(DUMP_WATER_LEVEL_MEASUREMENTS)
  initAnalogPins();
  deinitUnusedAnalogPins();
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
    uint16_t rawMeasurement;
    waterTankNotEmpty(waterPin, waterMin, waterMax, waterEmpty, measurement,
                      rawMeasurement);

    // Finally turn the power of the sensors and the pump off
    shiftReg.disableOutput();
  }
#else
  // ===================================================================
  // Production mode
  // ===================================================================

  SERIALprintlnP(PSTR("Running irrigation routine!"));

  // Switch to normal input mode early to ensure enough time to discharge all
  // caps
  initAnalogPins();
  // allow remote to reset the hardware failure flag
  if (powerUpEthernet(shiftReg)) {
    updateSettings(settings);
  }
  deinitUnusedAnalogPins();
  powerDownEthernet(shiftReg);

  uint16_t secondsPassed = 0;
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

    SERIALprintlnP(PSTR("Irrigation running!"));
    uint8_t idx = 0;
    for (; !settings.hardwareFailure && idx < settings.numPlants; ++idx) {
      bool skip = status.ticksSinceIrrigation[idx] <
                      settings.ticksBetweenIrrigation[idx] ||
                  ((settings.skipBitmap[idx / 8] >> (idx & 7 /*aka mod 8*/)) &
                   0x01) != 0;
      statusChanged |= !skip;
      if (!skip) {
        // This ensures that the tick counter will overflow to 0 -> reset its
        // value!
        status.ticksSinceIrrigation[idx] = 255;
        resCode |= checkMoisture(idx, status, secondsPassed);
      }
    }

    shiftReg.disableOutput();
    shiftReg.update(0);
    analogPowerSave();

    // Update the tick counters!
    for (auto &t : status.ticksSinceIrrigation) {
      ++t;
    }

    // Only send messages if the status changed!
    statusChanged |= settings.debug;
    SERIALprintP(PSTR("Status changed: "));
    SERIALprintln(statusChanged);
    if (statusChanged || settings.hardwareFailure) {
      SERIALprintlnP(PSTR("Send status updates!"));
      if (powerUpEthernet(shiftReg)) {
        if (statusChanged) {
          sendStatus(status);
          for (uint8_t i = 0; resCode != 0 && i < 2; ++i, resCode >>= 2) {
            if (resCode & 0x02) {
              sendErrorWaterEmpty(i);
            } else if (resCode & 0x01) {
              sendWarning(i);
            }
          }
        }
        if (settings.hardwareFailure) {
          // Send HW error message!
          sendErrorHardware(idx - 1);
        }
      }
      powerDownEthernet(shiftReg);
    }
  } else {
    analogPowerSave();
    // Update the tick counters!
    for (auto &t : status.ticksSinceIrrigation) {
      // prevent overflows!
      t = max(t, t + 1);
    }
  }
  SERIALprintlnP(PSTR("Entering long sleep!"));
  sleepSec(SLEEP_PERIOD_MIN * 60 - secondsPassed);
#endif
}
