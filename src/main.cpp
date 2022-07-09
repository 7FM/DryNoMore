#include "config.hpp"
#include "sleep.hpp"

#include <Arduino.h>

// Helper macros
#define CONST_ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

// Vars
static constexpr decltype(A0) moisturePins[] = MOISTURE_SENSOR_PINS;
static constexpr decltype(A0) moisturePowerPins[] = MOISTURE_SENSOR_POWER_PINS;
static constexpr decltype(A0) mosfetPins[] = MOSFET_PINS;
static constexpr uint16_t moistureMin[] = MOISTURE_MIN_VALUES;
static constexpr uint16_t moistureMax[] = MOISTURE_MAX_VALUES;
static constexpr uint8_t moistureTarget[] = MOISTURE_TARGETS;

// Global vars
static bool hardwareFailure = false;

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
    qsort(measurements, CONST_ARRAY_SIZE(measurements), sizeof(measurements[0]), uint16_comp);

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

static inline bool waterTankNotEmpty() {
    // TODO check
    // TODO notify somehow the user that we need more water!!!
    return false;
}

static inline bool isSoilTooDry(uint8_t pin, uint16_t min, uint16_t max, uint8_t target) {
    uint16_t value = adcMeasurement(pin);
    // clamp the value first
    value = constrain(value, min, max);
    uint8_t moisturePercentage = map(value, min, max, 0, 100);
    return moisturePercentage < target;
}

void checkMoisture(uint8_t idx) {
    const auto powerPin = moisturePowerPins[idx];
    digitalWrite(powerPin, HIGH);
    _delay_ms(POWER_ON_DELAY_MS);

    const auto pumpPin = mosfetPins[idx];
    const auto pin = moisturePins[idx];
    const auto min = moistureMin[idx];
    const auto max = moistureMax[idx];
    const auto target = moistureTarget[idx];

    // timeout to ensure we are not stuck here forever (and flood the plants) in case of an defect sensor/pump
    constexpr long measurementDuration = (static_cast<long>(ADC_MEASUREMENTS) * (MEASURE_DELAY_MS));
    constexpr uint8_t timeoutCycles = (static_cast<long>(IRRIGATION_TIMEOUT_SEC) * 1000 + measurementDuration) / measurementDuration;

    for (uint8_t cycle = 0; waterTankNotEmpty() && isSoilTooDry(pin, min, max, target); ++cycle) {
        if (cycle == timeoutCycles) {
            hardwareFailure = true;
            // TODO in case the timeout was triggered, we should ensure that the irrigation
            // wont be triggered again until the system was reset & the problem fixed by an user
            // TODO report error and prevent future execution!
            break;
        }
        // turn on the pump
        digitalWrite(pumpPin, HIGH);
    }

    // Finally turn the power of the sensor and the pump off
    digitalWrite(powerPin, LOW);
    digitalWrite(pumpPin, LOW);
}

static void disableDigitalOnAnalogPins() {
    // disable digital input buffer on analog pins, as not used and saves energy
    DIDR0 = _BV(ADC0D) | _BV(ADC1D) | _BV(ADC2D) | _BV(ADC3D) | _BV(ADC4D) | _BV(ADC5D);
    DIDR1 = _BV(AIN0D) | _BV(AIN1D);
}

void setup() {
    // Sanity checks
    static_assert(CONST_ARRAY_SIZE(moisturePins) == CONST_ARRAY_SIZE(moisturePowerPins), "Each moisture sensor needs a dedicated power pin!");
    static_assert(CONST_ARRAY_SIZE(moisturePins) == CONST_ARRAY_SIZE(mosfetPins), "You need to specify as many mosfet pins as available moisture sensors!");
    static_assert(CONST_ARRAY_SIZE(moisturePins) == CONST_ARRAY_SIZE(moistureMin), "You need to specify as many min moisture values as available moisture sensors!");
    static_assert(CONST_ARRAY_SIZE(moisturePins) == CONST_ARRAY_SIZE(moistureMax), "You need to specify as many max moisture values as available moisture sensors!");
    static_assert(CONST_ARRAY_SIZE(moisturePins) == CONST_ARRAY_SIZE(moistureTarget), "You need to specify as many target moisture values as available moisture sensors!");

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

    Serial.begin(9600);

    for (uint8_t i = 0; i < CONST_ARRAY_SIZE(moisturePins); ++i) {
        // initialize all pins
        // TODO does this result in a lower power consumption?
        pinMode(moisturePins[i], INPUT_PULLUP);

        pinMode(moisturePowerPins[i], OUTPUT);
        pinMode(mosfetPins[i], OUTPUT);
        // Turn the sensor off
        digitalWrite(moisturePowerPins[i], LOW);
        // Turn the pump off
        digitalWrite(mosfetPins[i], LOW);
    }

    disableDigitalOnAnalogPins();
}

void loop() {
    // Check all plants!
    for (uint8_t idx = 0; !hardwareFailure && idx < CONST_ARRAY_SIZE(moisturePins); ++idx) {
        checkMoisture(idx);
    }
    longSleep<SLEEP_PERIOD_MIN>();
}
