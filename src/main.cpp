#include "config.hpp"

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
static bool hardwareFailure = false;
static ShiftReg shiftReg;
static Settings settings;

static int uint16_comp(const void *i1, const void *i2) {
  uint16_t val1 = *reinterpret_cast<const uint16_t *>(i1);
  uint16_t val2 = *reinterpret_cast<const uint16_t *>(i2);
  // order doesn't really matter as we are only interested in the median anyways
  return val1 - val2;
}

uint16_t adcMeasurement(uint8_t pin) {
  uint16_t measurements[ADC_MEASUREMENTS];

  // take the measurements
  measurements[0] = analogRead(pin);
  for (uint8_t i = 1; i < (ADC_MEASUREMENTS); ++i) {
    _delay_ms(MEASURE_DELAY_MS);
    measurements[i] = analogRead(pin);
  }

  // sort the measurements
  // use the quicksort algorithm provided by the avr library
  qsort(measurements, (ADC_MEASUREMENTS), sizeof(measurements[0]), uint16_comp);

  // determine the median
  uint16_t median;

  // the compiler should be smart enough to optimize this away.
  // otherwise it is garbage!
  if (((ADC_MEASUREMENTS) % 2) == 0) {
    median = (measurements[(ADC_MEASUREMENTS) / 2 - 1] +
              measurements[(ADC_MEASUREMENTS) / 2]) /
             2;
  } else {
    median = measurements[(ADC_MEASUREMENTS) / 2];
  }

  return median;
}

static uint8_t clampedMeasurement(uint8_t pin, uint16_t min, uint16_t max) {
  uint16_t value = adcMeasurement(pin);
  SERIALprintP(PSTR(" raw: "));
  SERIALprint(value);
  SERIALprintP(PSTR(" clamped: "));
  // clamp the value first
  value = constrain(value, min, max);
  // inverted mapping as an increase in moisture/water level results in a lower
  // ADC value
  uint8_t percentage = 100 - map(value, min, max, 0, 100);

  SERIALprint(percentage);
  SERIALprintP(PSTR(" % for pin: "));
  SERIALprintln(pin);

  return percentage;
}

static inline bool isSoilTooDry(uint8_t pin, uint16_t min, uint16_t max,
                                uint8_t target) {
  SERIALprintP(PSTR("Measured soil moisture"));
  uint8_t moisturePercentage = clampedMeasurement(pin, min, max);
  return moisturePercentage < target;
}

static inline bool waterTankNotEmpty(uint8_t pin, uint16_t min, uint16_t max,
                                     uint8_t warning, uint8_t empty) {
  SERIALprintP(PSTR("Measured water tank level"));
  uint8_t waterLevelPercentage = clampedMeasurement(pin, min, max);
  bool isEmpty = waterLevelPercentage < empty;

  if (isEmpty) {
    // TODO send error to user that the tank is empty!
    sendErrorWaterEmpty();
  } else if (waterLevelPercentage <= warning) {
    // TODO notify somehow the user that we need more water!!!
    sendWarning();
  }

  return !isEmpty;
}

void checkMoisture(uint8_t idx) {
  const auto pumpMask = pumpPwrMap[idx];
  const auto moistPin = moistSensPins[idx];
  const auto moistSensMask = moistSensPwrMap[idx];

  const auto moistMin = settings.sensConfs[idx].minValue;
  const auto moistMax = settings.sensConfs[idx].maxValue;
  const auto moistTarget = settings.targetMoistures[idx];

  const uint8_t waterSensIdx =
      (settings.moistSensToWaterSensBitmap[idx / 8] >> idx) & 0x01;

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

  for (uint8_t cycle = 0;
       waterTankNotEmpty(waterPin, waterMin, waterMax, waterWarning,
                         waterEmpty) &&
       isSoilTooDry(moistPin, moistMin, moistMax, moistTarget);
       ++cycle) {
    if (cycle == timeoutCycles) {
      hardwareFailure = true;
      sendErrorHardware();
      shiftReg.disableOutput();
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
}

static void disableDigitalOnAnalogPins() {
  // disable digital input buffer on analog pins, as not used and saves energy
  DIDR0 = _BV(ADC0D) | _BV(ADC1D) | _BV(ADC2D) | _BV(ADC3D) | _BV(ADC4D) |
          _BV(ADC5D);
  DIDR1 = _BV(AIN0D) | _BV(AIN1D);
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

  SERIALbegin(SERIAL_BAUD_RATE);
  defaultInitSettings(settings);

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

  disableDigitalOnAnalogPins();
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

    isSoilTooDry(moistPin, moistMin, moistMax, moistTarget);

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
    const auto waterWarning = settings.waterLvlThres[idx].warnThres;
    const auto waterEmpty = settings.waterLvlThres[idx].emptyThres;

    shiftReg.update(waterSensMask);
    shiftReg.enableOutput();
    _delay_ms(POWER_ON_DELAY_MS);

    waterTankNotEmpty(waterPin, waterMin, waterMax, waterWarning, waterEmpty);

    // Finally turn the power of the sensors and the pump off
    shiftReg.disableOutput();
  }
#else
  // ===================================================================
  // Production mode
  // ===================================================================

  // Check all plants!
  // TODO allow remote reseting hardware failure?
  // TODO add skip setting to skip certain plants as immediate workaround of
  // hardware problems?
  if (!hardwareFailure) {
    initEthernet(); // TODO enable Ethernet only on demand!
    updateSettings(settings);

    shiftReg.update(0);
    shiftReg.enableOutput();

    for (uint8_t idx = 0; !hardwareFailure && idx < settings.numPlants; ++idx) {
      checkMoisture(idx);
    }

    shiftReg.disableOutput();
    shiftReg.update(0);
    sendStatus();
    deinitEthernet();
  }
  longSleep<SLEEP_PERIOD_MIN>();
#endif
}
