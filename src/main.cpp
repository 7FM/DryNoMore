#include "config.hpp"

#include "power_ctrl.hpp"
#include "progmem_strings.hpp"
#include "sleep.hpp"

#include <Arduino.h>

// Helper macros
#define CONST_ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

// Consts
static constexpr decltype(A0) moistSensPins[] = MOISTURE_SENSOR_PINS;
static constexpr uint16_t moistSensMin[] = MOISTURE_MIN_VALUES;
static constexpr uint16_t moistSensMax[] = MOISTURE_MAX_VALUES;
static constexpr uint8_t moistureTarget[] = MOISTURE_TARGET_THRESHOLDS;

static constexpr decltype(A0) waterSensPins[] = WATER_SENSOR_PINS;
static constexpr uint16_t waterSensMin[] = WATER_MIN_VALUES;
static constexpr uint16_t waterSensMax[] = WATER_MAX_VALUES;
static constexpr uint8_t waterWarningThres[] = WATER_WARNING_THRESHOLDS;
static constexpr uint8_t waterEmptyThres[] = WATER_EMPTY_THRESHOLDS;

static constexpr uint8_t usedMoistSens = CONST_ARRAY_SIZE(moistureTarget);
static constexpr uint8_t usedWaterSens = CONST_ARRAY_SIZE(waterWarningThres);

static constexpr uint16_t moistSensPwrMap[] = MOIST_SENS_PWR_MAPPING;
static constexpr uint16_t waterSensPwrMap[] = WATER_SENS_PWR_MAPPING;
static constexpr uint16_t pumpPwrMap[] = PUMP_PWR_MAPPING;

// Global vars
static bool hardwareFailure = false;
static ShiftReg shiftReg;

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
        median = (measurements[(ADC_MEASUREMENTS) / 2 - 1] + measurements[(ADC_MEASUREMENTS) / 2]) / 2;
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
    uint8_t percentage = map(value, min, max, 0, 100);

    SERIALprint(percentage);
    SERIALprintP(PSTR(" % for pin: "));
    SERIALprintln(pin);

    return percentage;
}

static inline bool isSoilTooDry(uint8_t pin, uint16_t min, uint16_t max, uint8_t target) {
    SERIALprintP(PSTR("Measured soil moisture "));
    uint8_t moisturePercentage = clampedMeasurement(pin, min, max);
    return moisturePercentage < target;
}

static inline bool waterTankNotEmpty(uint8_t pin, uint16_t min, uint16_t max, uint8_t warning, uint8_t empty) {
    SERIALprintP(PSTR("Measured water tank level "));
    uint8_t waterLevelPercentage = clampedMeasurement(pin, min, max);
    bool isEmpty = waterLevelPercentage < empty;

    if (waterLevelPercentage < warning) {
        // TODO notify somehow the user that we need more water!!!
    }

    return !isEmpty;
}

void checkMoisture(uint8_t idx) {
    const auto pumpMask = pumpPwrMap[idx];
    const auto moistPin = moistSensPins[idx];
    const auto moistSensMask = moistSensPwrMap[idx];

    const auto moistMin = moistSensMin[idx];
    const auto moistMax = moistSensMax[idx];
    const auto moistTarget = moistureTarget[idx];

    // TODO when to use sensor 2?
    const auto waterPin = waterSensPins[0];
    const auto waterSensMask = waterSensPwrMap[0];
    const auto waterMin = waterSensMin[0];
    const auto waterMax = waterSensMax[0];
    const auto waterWarning = waterWarningThres[0];
    const auto waterEmpty = waterEmptyThres[0];

    shiftReg.update(moistSensMask | waterSensMask);
    _delay_ms(POWER_ON_DELAY_MS);
    bool isPumpActive = false;

    // timeout to ensure we are not stuck here forever (and flood the plants) in case of an defect sensor/pump
    constexpr long measurementDuration = (static_cast<long>(ADC_MEASUREMENTS) * (MEASURE_DELAY_MS));
    constexpr uint8_t timeoutCycles = (static_cast<long>(IRRIGATION_TIMEOUT_SEC) * 1000 + measurementDuration) / measurementDuration;

    for (uint8_t cycle = 0; waterTankNotEmpty(waterPin, waterMin, waterMax, waterWarning, waterEmpty) &&
                            isSoilTooDry(moistPin, moistMin, moistMax, moistTarget);
         ++cycle) {
        if (cycle == timeoutCycles) {
            hardwareFailure = true;
            // TODO in case the timeout was triggered, we should ensure that the irrigation
            // wont be triggered again until the system was reset & the problem fixed by an user
            // TODO report error and prevent future execution!
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
    DIDR0 = _BV(ADC0D) | _BV(ADC1D) | _BV(ADC2D) | _BV(ADC3D) | _BV(ADC4D) | _BV(ADC5D);
    DIDR1 = _BV(AIN0D) | _BV(AIN1D);
}

void setup() {
    // Sanity checks
    static_assert(CONST_ARRAY_SIZE(moistureTarget) == CONST_ARRAY_SIZE(moistSensMin), "You need to specify as many min moisture values as available moisture sensor targets!");
    static_assert(CONST_ARRAY_SIZE(moistureTarget) == CONST_ARRAY_SIZE(moistSensMax), "You need to specify as many max moisture values as available moisture sensor targets!");
    static_assert(CONST_ARRAY_SIZE(moistSensPins) == CONST_ARRAY_SIZE(moistSensPwrMap), "You need to specify as many moisture sensor pins as moisture sensor power control mappings!");
    static_assert(CONST_ARRAY_SIZE(waterWarningThres) == CONST_ARRAY_SIZE(waterSensMin), "You need to specify as many min water level values as available water sensor targets!");
    static_assert(CONST_ARRAY_SIZE(waterWarningThres) == CONST_ARRAY_SIZE(waterSensMax), "You need to specify as many max water level values as available water sensor targets!");
    static_assert(CONST_ARRAY_SIZE(waterWarningThres) == CONST_ARRAY_SIZE(waterEmptyThres), "You need to specify as many empty water level values as available water sensor targets!");
    static_assert(CONST_ARRAY_SIZE(waterSensPins) == CONST_ARRAY_SIZE(waterSensPwrMap), "You need to specify as many water level sensor pins as water level sensor power control mappings!");
    static_assert(usedMoistSens <= CONST_ARRAY_SIZE(pumpPwrMap), "Each configured moisture sensor needs one pump!");

    noInterrupts();
    // TODO reduce the clock speed for a lower power consumption
    // To avoid unintentional changes of clock frequency, a special write procedure must be followed to change the CLKPS bits:
    // 1. Write the clock prescaler change enable (CLKPCE) bit to one and all other bits in CLKPR to zero.
    CLKPR = _BV(CLKPCE);
    // 2. Within four cycles, write the desired value to CLKPS while writing a zero to CLKPCE.
    // division factor of 256!!! We are getting really really slow... 16 MHz / 256 = 62.5 kHz
    // TODO check whether delay() resp. _delay_ms() accounts for these changes?
    CLKPR = _BV(CLKPS3);
    interrupts();

    SERIALbegin(9600);

    // TODO update!
    for (uint8_t i = 0; i < usedMoistSens; ++i) {
        // initialize all pins
        // TODO does this result in a lower power consumption?
        pinMode(moistSensPins[i], INPUT_PULLUP);
    }
    for (uint8_t i = 0; i < usedWaterSens; ++i) {
        // initialize all pins
        // TODO does this result in a lower power consumption?
        pinMode(waterSensPins[i], INPUT_PULLUP);
    }

    disableDigitalOnAnalogPins();
}

void loop() {

    // ===================================================================
    // Setup modes
    // ===================================================================
#ifdef DUMP_SOIL_MOISTURES_MEASUREMENTS
    for (uint8_t idx = 0; idx < usedMoistSens; ++idx) {
        const auto moistPin = moistSensPins[idx];
        const auto moistSensMask = moistSensPwrMap[idx];

        const auto moistMin = moistSensMin[idx];
        const auto moistMax = moistSensMax[idx];
        const auto moistTarget = moistureTarget[idx];

        shiftReg.update(moistSensMask);
        shiftReg.enableOutput();
        _delay_ms(POWER_ON_DELAY_MS);

        isSoilTooDry(moistPin, moistMin, moistMax, moistTarget);

        // Finally turn the power of the sensors and the pump off
        shiftReg.disableOutput();
    }
#elif defined(DUMP_WATER_LEVEL_MEASUREMENTS)
    for (uint8_t idx = 0; idx < usedWaterSens; ++idx) {
        const auto waterPin = waterSensPins[idx];
        const auto waterSensMask = waterSensPwrMap[idx];
        const auto waterMin = waterSensMin[idx];
        const auto waterMax = waterSensMax[idx];
        const auto waterWarning = waterWarningThres[idx];
        const auto waterEmpty = waterEmptyThres[idx];

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
    if (!hardwareFailure) {
        shiftReg.update(0);
        shiftReg.enableOutput();

        for (uint8_t idx = 0; !hardwareFailure && idx < usedMoistSens; ++idx) {
            checkMoisture(idx);
        }

        shiftReg.disableOutput();
        shiftReg.update(0);
    }
    longSleep<SLEEP_PERIOD_MIN>();
#endif
}
