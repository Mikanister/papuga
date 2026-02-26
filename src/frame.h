#ifndef FRAME_H
#define FRAME_H

#include <stdbool.h>
#include <stdint.h>

constexpr uint8_t PING_FRAME_LEN = 9U;
constexpr uint8_t REPORT_TYPE = 0x10U;
constexpr uint8_t TLV_FREQ_LIST = 0x01U;
constexpr uint8_t TLV_NODE_STATUS = 0x02U;

void buildPingFrame(uint16_t seq, uint8_t out[PING_FRAME_LEN]);
bool parsePingFrame(const uint8_t* buf,
                    uint8_t len,
                    uint16_t& seqOut,
                    uint8_t& srcOut,
                    uint8_t& bootOut,
                    uint8_t& errCode);

uint8_t buildReportFrame(uint16_t seq,
                         uint8_t dstId,
                         const uint16_t* freqMHz,
                         uint8_t freqCount,
                         uint8_t statusFlags,
                         uint16_t lastUartAgeS,
                         uint8_t* out,
                         uint8_t outMax);

#endif  // FRAME_H
