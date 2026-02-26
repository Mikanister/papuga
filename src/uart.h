#ifndef UART_H
#define UART_H

#include <stdint.h>

enum class UartRxState : uint8_t {
  UART_IDLE = 0,
  UART_COLLECT,
  UART_READY,
  UART_OVERRUN,
};

void uartInit();
void uartPoll(uint32_t nowMs);

UartRxState uartState();
bool uartHasValidLine();
const char* uartLastValidLine();
uint32_t uartLastTimestampMs();

#endif  // UART_H
