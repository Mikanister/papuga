#include "log.h"

#include <Arduino.h>

#if LOG_ENABLED

void logInit() {
  Serial.begin(UART_BAUD);
}

void logEvent(const char* tag) {
  Serial.println(tag);
}

void logEvent2(const char* tag, int32_t v) {
  Serial.print(tag);
  Serial.print(' ');
  Serial.println(v);
}

void logEvent3(const char* tag, int32_t v1, int32_t v2) {
  Serial.print(tag);
  Serial.print(' ');
  Serial.print(v1);
  Serial.print(' ');
  Serial.println(v2);
}

void logHex8(const char* tag, const uint8_t data[8]) {
  Serial.print(tag);
  for (uint8_t i = 0; i < 8; ++i) {
    Serial.print(' ');
    if (data[i] < 16U) {
      Serial.print('0');
    }
    Serial.print(data[i], HEX);
  }
  Serial.println();
}

#else

void logInit() {
}

void logEvent(const char*) {
}

void logEvent2(const char*, int32_t) {
}

void logEvent3(const char*, int32_t, int32_t) {
}

void logHex8(const char*, const uint8_t[8]) {
}

#endif
