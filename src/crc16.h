#ifndef CRC16_H
#define CRC16_H

#include <stdint.h>

uint16_t crc16_ccitt_false(const uint8_t* data, uint8_t len);

#endif  // CRC16_H
