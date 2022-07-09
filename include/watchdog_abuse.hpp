#pragma once

#include "config.hpp"

#ifndef WATCHDOG_DURATION
#define WATCHDOG_PRESCALE 1024 // 2,4,8,16,32,64,128,256,512,1024
#endif

// Time-out Interval:
// 16ms
#if WATCHDOG_PRESCALE == 2
#define WATCHDOG_PRESCALE_MASK 0

// 32ms
#elif WATCHDOG_PRESCALE == 4
#define WATCHDOG_PRESCALE_MASK _BV(WDP0)

// 64ms
#elif WATCHDOG_PRESCALE == 8
#define WATCHDOG_PRESCALE_MASK _BV(WDP1)

// 125ms
#elif WATCHDOG_PRESCALE == 16
#define WATCHDOG_PRESCALE_MASK _BV(WDP0) | _BV(WDP1)

// 250ms
#elif WATCHDOG_PRESCALE == 32
#define WATCHDOG_PRESCALE_MASK _BV(WDP2)

// 500ms
#elif WATCHDOG_PRESCALE == 64
#define WATCHDOG_PRESCALE_MASK _BV(WDP2) | _BV(WDP0)

// 1s
#elif WATCHDOG_PRESCALE == 128
#define WATCHDOG_PRESCALE_MASK _BV(WDP2) | _BV(WDP1)
#define WATCHDOG_DURATION_SEC 1

// 2s
#elif WATCHDOG_PRESCALE == 256
#define WATCHDOG_PRESCALE_MASK _BV(WDP2) | _BV(WDP1) | _BV(WDP0)
#define WATCHDOG_DURATION_SEC 2

// 4s
#elif WATCHDOG_PRESCALE == 512
#define WATCHDOG_PRESCALE_MASK _BV(WDP3)
#define WATCHDOG_DURATION_SEC 4

// 8s
#elif WATCHDOG_PRESCALE == 1024
#define WATCHDOG_PRESCALE_MASK _BV(WDP3) | _BV(WDP0)
#define WATCHDOG_DURATION_SEC 8

#else
#error "Invalid watchdog prescale!"
#endif

#if WATCHDOG_PRESCALE < 128
#error "The watchdog prescale is too small to use for an long sleep!"
#endif

void startWatchDogTimer();
void stopWatchDogTimer();
