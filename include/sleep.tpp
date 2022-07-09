#pragma once

#include "watchdog_abuse.hpp"

namespace {
    static void enterSleepMode() {
        // TODO what do we use to wake up again?
        // the Watchdog Timer seems to have its own oscillator and has a max delay of 8s
        // TODO maybe we can achieve longer sleeps with a normal timer and the slow system clock of 62.5 kHz
        // TODO the later would require a different sleep mode?
        // TODO Timer2 can use clk_io if AS2 is set appropriately
        // TODO it also allows for an additional prescaler of 1024 -> 62.5 kHz / 1024 = approx. 61 Hz -> 8 bit Timer -> approx. 4 seconds until the interrupt
        // TODO when using async mode with the internal oscillator: 32.768kHz TODO the system clock needs to be at least 4 times higher... see page 126
        // TODO 32.768 kHz / 1024 = 32 Hz -> 8 seconds... so in either case we do not gain any advantage compared to the watchdog solution...
        // TODO Hence, abuse the watchdog timer with an additional uint16_t counter variable to run the moisture checks every X hours

        // disable ADC by clearing the enable bit
        ADCSRA &= ~_BV(ADEN);
        // disable the analog comparator interrupts
        ACSR &= ~_BV(ACIE);
        // disable the analog comparator by setting the disable flag
        ACSR |= _BV(ACD);

        // select the sleep mode:
        SMCR = SLEEP_MODE_PWR_DOWN | _BV(SE);

        // disable the BOD (brownout detection) by software! this further reduces the power consumption
        // Writing to the BODS bit is controlled by a timed sequence and an enable bit, BODSE in MCUCR.
        // To disable BOD in relevant sleep modes, both BODS and BODSE must first be set to one.
        MCUCR |= _BV(BODS) | _BV(BODSE);
        // Then, to set the BODS bit, BODS must be set to one and BODSE must be set to zero within four clock cycles.
        MCUCR = (MCUCR & (~_BV(BODSE))) | _BV(BODS);
        // TODO count cycles of the produces assembler code or write directly in assembly!

        // The BODS bit is active three clock cycles after it is set.
        // A sleep instruction must be executed while BODS is active in order to turn off the BOD for the actual sleep mode.
        // The BODS bit is automatically cleared after three clock cycles.
        // -> we need to go immediately into a sleep mode
        __asm__ __volatile__("sleep");
        // Clear sleep enable to avoid unwanted sleeps!
        SMCR &= ~_BV(SE);

        // enable ADC again
        ADCSRA |= _BV(ADEN);
    }
} // namespace

template <uint16_t duration_in_min>
void longSleep() {
    uint16_t watchdogTicks = 0;

    constexpr uint64_t sleepTicksLargeVar = (static_cast<uint64_t>(duration_in_min) * 60 + static_cast<uint64_t>(WATCHDOG_DURATION_SEC) - 1) / static_cast<uint64_t>(WATCHDOG_DURATION_SEC);
    static_assert(sleepTicksLargeVar < 65536ULL, "To long sleep duration specified!");
    constexpr decltype(watchdogTicks) neededSleepTicks = static_cast<uint16_t>(sleepTicksLargeVar);

    startWatchDogTimer();
    do {
        enterSleepMode();
        ++watchdogTicks;
    } while (watchdogTicks != neededSleepTicks);
    stopWatchDogTimer();
}
