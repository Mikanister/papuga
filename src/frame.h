#ifndef FRAME_H
#define FRAME_H

#include <stdbool.h>
#include <stdint.h>

constexpr uint8_t PING_FRAME_LEN = 12U;
constexpr uint8_t REPORT_TYPE = 0x10U;
constexpr uint8_t TLV_FREQ_LIST = 0x01U;
constexpr uint8_t TLV_NODE_STATUS = 0x02U;
constexpr uint8_t FRAME_FLAG_NO_RELAY = 0x01U;

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

bool frameCrcOk(const uint8_t* buf, uint8_t len);
bool frameGetSrcMsgId(const uint8_t* buf, uint8_t len, uint8_t& srcOut, uint16_t& msgIdOut);
bool frameGetTTL(const uint8_t* buf, uint8_t len, uint8_t& ttlOut);
bool frameGetHops(const uint8_t* buf, uint8_t len, uint8_t& hopsOut);
bool frameIsNoRelay(const uint8_t* buf, uint8_t len);
bool frameDecTTLIncHopsAndRecrc(uint8_t* buf, uint8_t len);

#endif  // FRAME_H
