#include "uart.h"

#include <Arduino.h>
#include <string.h>

#include "config.h"
#include "log.h"

namespace {

UartRxState gRxState = UartRxState::UART_IDLE;
char gLineBuf[UART_LINE_MAX] = {0};
uint8_t gLineLen = 0;

char lastValidLine[UART_LINE_MAX] = {0};
uint32_t last_uart_timestamp_ms = 0;
bool uartValid = false;

void resetCollector() {
  gLineLen = 0;
  gLineBuf[0] = '\0';
}

void setOverrun() {
  gRxState = UartRxState::UART_OVERRUN;
  resetCollector();
  logEvent("UOVR");
}

void finalizeLine(uint32_t nowMs) {
  gLineBuf[gLineLen] = '\0';
  gRxState = UartRxState::UART_READY;
  logEvent2("UOK", gLineLen);

  memcpy(lastValidLine, gLineBuf, UART_LINE_MAX);
  last_uart_timestamp_ms = nowMs;
  uartValid = true;

  gRxState = UartRxState::UART_IDLE;
  resetCollector();
}

}  // namespace

void uartInit() {
  Serial.begin(UART_BAUD);
  gRxState = UartRxState::UART_IDLE;
  resetCollector();
  lastValidLine[0] = '\0';
  last_uart_timestamp_ms = 0;
  uartValid = false;
}

void uartPoll(uint32_t nowMs) {
  uint8_t processed = 0;
  while ((Serial.available() > 0) && (processed < UART_MAX_BYTES_PER_TICK)) {
    const int raw = Serial.read();
    if (raw < 0) {
      break;
    }
    ++processed;

    const char ch = static_cast<char>(raw);

    if (gRxState == UartRxState::UART_OVERRUN) {
      if (ch == '\n') {
        gRxState = UartRxState::UART_IDLE;
      }
      continue;
    }

    if (ch == '\r') {
      continue;
    }

    if (ch == '\n') {
      finalizeLine(nowMs);
      continue;
    }

    if (gRxState == UartRxState::UART_IDLE) {
      gRxState = UartRxState::UART_COLLECT;
    }

    if (gLineLen >= static_cast<uint8_t>(UART_LINE_MAX - 1U)) {
      setOverrun();
      continue;
    }

    gLineBuf[gLineLen] = ch;
    ++gLineLen;
  }
}

UartRxState uartState() {
  return gRxState;
}

bool uartHasValidLine() {
  return uartValid;
}

const char* uartLastValidLine() {
  return lastValidLine;
}

uint32_t uartLastTimestampMs() {
  return last_uart_timestamp_ms;
}
