#ifndef RADIO_H
#define RADIO_H

#include <stdbool.h>
#include <stdint.h>

bool radioInit();
bool radioSend(const uint8_t* data, uint8_t len);
bool radioStartRx();
bool radioIsIdle();
uint8_t radioRead(uint8_t* out, uint8_t maxLen);
int16_t radioLastRssi();
int8_t radioLastSnr();
uint8_t radioLastCode();

#endif  // RADIO_H
