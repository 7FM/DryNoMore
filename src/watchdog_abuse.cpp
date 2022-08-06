#include "watchdog_abuse.hpp"

#include <Arduino.h>

void startWatchDogTimer() {
  noInterrupts();

  // reset the watchdog timer
  __asm__ __volatile__("wdr");
  // We need to clear WDRF before we can clear WDE
  MCUSR &= ~_BV(WDRF);
  // Start timed sequence
  WDTCSR = _BV(WDCE) | _BV(WDE);
  // Overwrite bits with the wanted configuration
  // Set new prescaler(time-out) and enable interrupt mode
  WDTCSR = _BV(WDIF) | _BV(WDIE) | WATCHDOG_PRESCALE_MASK;

  interrupts();
}

void stopWatchDogTimer() {
  noInterrupts();

  // reset the watchdog timer
  __asm__ __volatile__("wdr");
  // We need to clear WDRF before we can clear WDE
  MCUSR &= ~_BV(WDRF);
  // Start timed sequence
  WDTCSR = _BV(WDCE) | _BV(WDE);
  // disable the watchdog
  WDTCSR = 0;

  interrupts();
}

// ISR_NOBLOCK let the compiler reenable interrupts ASAP
ISR(WDT_vect, ISR_NOBLOCK /*__attribute__((flatten))*/) {
  // Nothing to do here
}