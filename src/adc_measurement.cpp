#include "adc_measurement.hpp"
#include "config.hpp"
#include "serial.hpp"

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

uint8_t clampedMeasurement(uint8_t pin, uint16_t min, uint16_t max) {
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
