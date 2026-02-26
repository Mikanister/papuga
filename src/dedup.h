#ifndef DEDUP_H
#define DEDUP_H

#include <stdbool.h>
#include <stdint.h>

bool dedupSeen(uint8_t src, uint16_t msgId, uint32_t nowMs);
void dedupRemember(uint8_t src, uint16_t msgId, uint32_t nowMs);

#endif  // DEDUP_H
