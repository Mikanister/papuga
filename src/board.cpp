#include "board.h"

#include <Arduino.h>

#include "config.h"
#if LOG_ENABLED
#include "log.h"
#endif

namespace {

uint8_t gBootId = 0;
bool gBootIdReady = false;
constexpr uint8_t BATT_SAMPLES = 8;
constexpr uint16_t ADC_MAX_12BIT = 4095U;
constexpr uint16_t ADC_REF_MV = 3300U;

uint8_t generateBootId() {
  uint32_t mix = micros();
  for (uint8_t i = 0; i < 8; ++i) {
    mix ^= (micros() << (i & 7));
    mix ^= (static_cast<uint32_t>(analogRead(PIN_BATT_ADC)) << ((i * 3) & 15));
    delayMicroseconds(60 + (mix & 0x1F));
  }
  mix ^= (mix >> 16);
  mix ^= (mix >> 8);
  return static_cast<uint8_t>(mix & 0xFF);
}

}  // namespace

void boardLedSet(bool on) {
  digitalWrite(CFG_PIN_LED, on ? LOW : HIGH);
}

void boardLedPulse(uint16_t ms) {
  boardLedSet(true);
  delay(ms);
  boardLedSet(false);
}

uint8_t boardBootId() {
  return gBootId;
}

uint16_t battReadMv() {
  uint32_t sum = 0;
  for (uint8_t i = 0; i < BATT_SAMPLES; ++i) {
    sum += static_cast<uint32_t>(analogRead(PIN_BATT_ADC));
  }

  const uint32_t avgRaw = (sum + (BATT_SAMPLES / 2U)) / BATT_SAMPLES;
  const uint32_t mv = (avgRaw * ADC_REF_MV) / ADC_MAX_12BIT;
  return static_cast<uint16_t>(mv);
}

void boardInit() {
  pinMode(CFG_PIN_LED, OUTPUT);
  boardLedSet(false);
  pinMode(PIN_BATT_ADC, INPUT_ANALOG);
  analogReadResolution(12);

  if (!gBootIdReady) {
    gBootId = generateBootId();
    gBootIdReady = true;
  }

#if LOG_ENABLED
  logInit();
  logEvent3("BOOT", NODE_ID, boardBootId());
  logEvent2("ROLE", IS_GATEWAY ? 1 : 0);
#endif

  for (uint8_t i = 0; i < 3; ++i) {
    boardLedPulse(90);
    if (i < 2) {
      delay(110);
    }
  }
  boardLedSet(false);
}
