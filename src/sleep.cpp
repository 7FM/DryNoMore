#include "sleep.hpp"
#include "lan.hpp"
#include "serial.hpp"
#include "watchdog_abuse.hpp"

namespace {
  static void enterSleepMode() {
    // what do we use to wake up again?
    // the Watchdog Timer seems to have its own oscillator and has a max delay
    // of 8s maybe we can achieve longer sleeps with a normal timer and the slow
    // system clock of 62.5 kHz: TLDR; no we can't! the later would require a
    // different sleep mode? Timer2 can use clk_io if AS2 is set appropriately
    // it also allows for an additional prescaler of 1024 -> 62.5 kHz / 1024 =
    // approx. 61 Hz -> 8 bit Timer -> approx. 4 seconds until the interrupt
    // when using async mode with the internal oscillator: 32.768kHz BUT the
    // system clock needs to be at least 4 times higher... see page 126 32.768
    // kHz / 1024 = 32 Hz -> 8 seconds... so in either case we do not gain any
    // advantage compared to the watchdog solution... Hence, abuse the watchdog
    // timer with an additional uint16_t counter variable to run the moisture
    // checks every X hours

    // select the sleep mode:
    SMCR = SLEEP_MODE_PWR_DOWN | _BV(SE);

    // disable the BOD (brownout detection) by software! this further reduces
    // the power consumption Writing to the BODS bit is controlled by a timed
    // sequence and an enable bit, BODSE in MCUCR. To disable BOD in relevant
    // sleep modes, both BODS and BODSE must first be set to one.
    MCUCR |= _BV(BODS) | _BV(BODSE);
    // Then, to set the BODS bit, BODS must be set to one and BODSE must be set
    // to zero within four clock cycles.
    MCUCR = (MCUCR & (~_BV(BODSE))) | _BV(BODS);

    // The BODS bit is active three clock cycles after it is set.
    // A sleep instruction must be executed while BODS is active in order to
    // turn off the BOD for the actual sleep mode. The BODS bit is automatically
    // cleared after three clock cycles.
    // -> we need to go immediately into a sleep mode
    __asm__ __volatile__("sleep");
    // Clear sleep enable to avoid unwanted sleeps!
    SMCR &= ~_BV(SE);
  }
} // namespace

void sleepSec(uint16_t duration_in_sec) {
  uint16_t watchdogTicks = 0;
  decltype(watchdogTicks) neededSleepTicks =
      duration_in_sec / WATCHDOG_DURATION_SEC;
  uint8_t remainingSleepSecs = static_cast<uint8_t>(
      duration_in_sec - (neededSleepTicks * WATCHDOG_DURATION_SEC));

  // disable ADC by clearing the enable bit
  ADCSRA &= ~_BV(ADEN);
  // disable the analog comparator interrupts
  ACSR &= ~_BV(ACIE);
  // disable the analog comparator by setting the disable flag
  ACSR |= _BV(ACD);

  SERIALflush();
  SERIALend();

  // Save the PRR register
  uint8_t prevPRR = PRR;
  // Disable ALL modules
  PRR = 0xFF;

  if (neededSleepTicks != 0) {
    startWatchDogTimer();
    do {
      enterSleepMode();
      ++watchdogTicks;
    } while (watchdogTicks != neededSleepTicks);
    stopWatchDogTimer();
  }
  // use delay for the remaining seconds:
  for (uint8_t i = 0; i < remainingSleepSecs; ++i) {
    _delay_ms(1000);
  }

  // Restore previous module power states
  PRR = prevPRR;

  // enable ADC again
  ADCSRA |= _BV(ADEN);

  SERIALbegin(SERIAL_BAUD_RATE);
}
