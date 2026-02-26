#ifndef BOARD_H
#define BOARD_H

#include <stdint.h>

void boardInit();
void boardLedSet(bool on);
void boardLedPulse(uint16_t ms);
uint8_t boardBootId();
uint16_t battReadMv();

#endif  // BOARD_H
