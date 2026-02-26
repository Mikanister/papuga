#ifndef LOG_H
#define LOG_H

#include "config.h"
#include <stdint.h>

void logInit();
void logEvent(const char* tag);
void logEvent2(const char* tag, int32_t v);
void logEvent3(const char* tag, int32_t v1, int32_t v2);
void logHex8(const char* tag, const uint8_t data[8]);

#endif  // LOG_H
