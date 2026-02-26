#include "dedup.h"

#include "config.h"

namespace {

struct DedupEntry {
  uint8_t src = 0U;
  uint16_t msgId = 0U;
  uint32_t timestampMs = 0UL;
  bool used = false;
};

DedupEntry gDedup[DEDUP_N];
uint16_t gDedupNext = 0U;

}  // namespace

bool dedupSeen(uint8_t src, uint16_t msgId, uint32_t nowMs) {
  (void)nowMs;  // Reserved for future aging policy.

  for (uint16_t i = 0U; i < DEDUP_N; ++i) {
    if (!gDedup[i].used) {
      continue;
    }
    if ((gDedup[i].src == src) && (gDedup[i].msgId == msgId)) {
      return true;
    }
  }
  return false;
}

void dedupRemember(uint8_t src, uint16_t msgId, uint32_t nowMs) {
  gDedup[gDedupNext].src = src;
  gDedup[gDedupNext].msgId = msgId;
  gDedup[gDedupNext].timestampMs = nowMs;
  gDedup[gDedupNext].used = true;

  ++gDedupNext;
  if (gDedupNext >= DEDUP_N) {
    gDedupNext = 0U;
  }
}
