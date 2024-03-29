#pragma once

#include "config.hpp"

#include <avr/pgmspace.h>

#ifdef DISABLE_SERIAL
#define SERIALflush()

#define SERIALprint(...)

#define SERIALprintln(...)

#define SERIALavailable() 0

#define SERIALparseInt(...) 0

#define SERIALwrite(...)
#define SERIALreadBytes(...)

#define SERIALbegin(...)
#define SERIALend(...)

#else
#define SERIALflush() Serial.flush()

#define SERIALprint(...) Serial.print(__VA_ARGS__)

#define SERIALprintln(...) Serial.println(__VA_ARGS__)

#define SERIALavailable() Serial.available()

#define SERIALparseInt(...) Serial.parseInt(__VA_ARGS__)

#define SERIALwrite(...) Serial.write(__VA_ARGS__)
#define SERIALreadBytes(...) Serial.readBytes(__VA_ARGS__)

#define SERIALbegin(...) Serial.begin(__VA_ARGS__)

#define SERIALend() Serial.end()

#endif

#define SERIALprintP(str)                                                      \
  SERIALprint(reinterpret_cast<const __FlashStringHelper *>(str))

#define SERIALprintlnP(str)                                                    \
  SERIALprintln(reinterpret_cast<const __FlashStringHelper *>(str))
