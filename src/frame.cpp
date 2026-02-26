#include "frame.h"

#include "board.h"
#include "config.h"
#include "crc16.h"

namespace {

constexpr uint8_t PING_TYPE = 0x01U;
constexpr uint8_t SCANNER_DST_ID = 0xFFU;
constexpr uint8_t PING_LEN = PING_FRAME_LEN;
constexpr uint8_t HEADER_LEN = 10U;
constexpr uint8_t CRC_LEN = 2U;
constexpr uint8_t NODE_STATUS_LEN = 3U;
constexpr uint8_t IDX_NET = 0U;
constexpr uint8_t IDX_SRC = 1U;
constexpr uint8_t IDX_DST = 2U;
constexpr uint8_t IDX_BOOT = 3U;
constexpr uint8_t IDX_TYPE = 4U;
constexpr uint8_t IDX_SEQ_L = 5U;
constexpr uint8_t IDX_SEQ_H = 6U;
constexpr uint8_t IDX_TTL = 7U;
constexpr uint8_t IDX_HOPS = 8U;
constexpr uint8_t IDX_FLAGS = 9U;

}  // namespace

void buildPingFrame(uint16_t seq, uint8_t out[PING_FRAME_LEN]) {
  out[IDX_NET] = NET_ID;
  out[IDX_SRC] = NODE_ID;
  out[IDX_DST] = SCANNER_DST_ID;
  out[IDX_BOOT] = boardBootId();
  out[IDX_TYPE] = PING_TYPE;
  out[IDX_SEQ_L] = static_cast<uint8_t>(seq & 0xFFU);
  out[IDX_SEQ_H] = static_cast<uint8_t>((seq >> 8) & 0xFFU);
  out[IDX_TTL] = DATA_TTL;
  out[IDX_HOPS] = 0U;
  out[IDX_FLAGS] = 0U;

  const uint16_t crc = crc16_ccitt_false(out, HEADER_LEN);
  out[HEADER_LEN] = static_cast<uint8_t>(crc & 0xFFU);
  out[HEADER_LEN + 1U] = static_cast<uint8_t>((crc >> 8) & 0xFFU);
}

bool frameCrcOk(const uint8_t* buf, uint8_t len) {
  if ((buf == nullptr) || (len < (HEADER_LEN + CRC_LEN))) {
    return false;
  }
  const uint8_t crcIdx = static_cast<uint8_t>(len - CRC_LEN);
  const uint16_t crcCalc = crc16_ccitt_false(buf, crcIdx);
  const uint16_t crcIn = static_cast<uint16_t>(buf[crcIdx]) |
                         static_cast<uint16_t>(static_cast<uint16_t>(buf[crcIdx + 1U]) << 8);
  return crcCalc == crcIn;
}

bool frameGetSrcMsgId(const uint8_t* buf, uint8_t len, uint8_t& srcOut, uint16_t& msgIdOut) {
  if ((buf == nullptr) || (len < (HEADER_LEN + CRC_LEN))) {
    return false;
  }
  srcOut = buf[IDX_SRC];
  msgIdOut = static_cast<uint16_t>(buf[IDX_SEQ_L]) |
             static_cast<uint16_t>(static_cast<uint16_t>(buf[IDX_SEQ_H]) << 8);
  return true;
}

bool frameGetTTL(const uint8_t* buf, uint8_t len, uint8_t& ttlOut) {
  if ((buf == nullptr) || (len < (HEADER_LEN + CRC_LEN))) {
    return false;
  }
  ttlOut = buf[IDX_TTL];
  return true;
}

bool frameGetHops(const uint8_t* buf, uint8_t len, uint8_t& hopsOut) {
  if ((buf == nullptr) || (len < (HEADER_LEN + CRC_LEN))) {
    return false;
  }
  hopsOut = buf[IDX_HOPS];
  return true;
}

bool frameIsNoRelay(const uint8_t* buf, uint8_t len) {
  if ((buf == nullptr) || (len < (HEADER_LEN + CRC_LEN))) {
    return true;
  }
  return (buf[IDX_FLAGS] & FRAME_FLAG_NO_RELAY) != 0U;
}

bool frameDecTTLIncHopsAndRecrc(uint8_t* buf, uint8_t len) {
  if ((buf == nullptr) || (len < (HEADER_LEN + CRC_LEN))) {
    return false;
  }
  if (!frameCrcOk(buf, len)) {
    return false;
  }
  if (buf[IDX_TTL] == 0U) {
    return false;
  }

  buf[IDX_TTL] = static_cast<uint8_t>(buf[IDX_TTL] - 1U);
  buf[IDX_HOPS] = static_cast<uint8_t>(buf[IDX_HOPS] + 1U);

  const uint8_t crcIdx = static_cast<uint8_t>(len - CRC_LEN);
  const uint16_t crc = crc16_ccitt_false(buf, crcIdx);
  buf[crcIdx] = static_cast<uint8_t>(crc & 0xFFU);
  buf[crcIdx + 1U] = static_cast<uint8_t>((crc >> 8) & 0xFFU);
  return true;
}

bool parsePingFrame(const uint8_t* buf,
                    uint8_t len,
                    uint16_t& seqOut,
                    uint8_t& srcOut,
                    uint8_t& bootOut,
                    uint8_t& errCode) {
  if (len != PING_LEN) {
    errCode = 1U;
    return false;
  }

  if (buf[0] != NET_ID) {
    errCode = 2U;
    return false;
  }

  if (buf[4] != PING_TYPE) {
    errCode = 4U;
    return false;
  }

  if (!frameCrcOk(buf, len)) {
    errCode = 3U;
    return false;
  }

  seqOut = static_cast<uint16_t>(buf[IDX_SEQ_L]) |
           static_cast<uint16_t>(static_cast<uint16_t>(buf[IDX_SEQ_H]) << 8);
  srcOut = buf[IDX_SRC];
  bootOut = buf[IDX_BOOT];
  errCode = 0U;
  return true;
}

uint8_t buildReportFrame(uint16_t seq,
                         uint8_t dstId,
                         const uint16_t* freqMHz,
                         uint8_t freqCount,
                         uint8_t statusFlags,
                         uint16_t lastUartAgeS,
                         uint8_t* out,
                         uint8_t outMax) {
  if ((out == nullptr) || (outMax < (HEADER_LEN + CRC_LEN))) {
    return 0U;
  }

  uint8_t safeFreqCount = freqCount;
  if (safeFreqCount > MAX_FREQS) {
    safeFreqCount = MAX_FREQS;
  }

  const uint8_t freqBytes = static_cast<uint8_t>(safeFreqCount * 2U);
  const uint8_t payloadLen =
      static_cast<uint8_t>(2U + freqBytes + 2U + NODE_STATUS_LEN);  // two TLV headers + payloads
  const uint8_t fullLen = static_cast<uint8_t>(HEADER_LEN + payloadLen + CRC_LEN);

  if (fullLen > outMax) {
    return 0U;
  }

  out[IDX_NET] = NET_ID;
  out[IDX_SRC] = NODE_ID;
  out[IDX_DST] = dstId;
  out[IDX_BOOT] = boardBootId();
  out[IDX_TYPE] = REPORT_TYPE;
  out[IDX_SEQ_L] = static_cast<uint8_t>(seq & 0xFFU);
  out[IDX_SEQ_H] = static_cast<uint8_t>((seq >> 8) & 0xFFU);
  out[IDX_TTL] = DATA_TTL;
  out[IDX_HOPS] = 0U;
  out[IDX_FLAGS] = 0U;

  uint8_t idx = HEADER_LEN;

  out[idx++] = TLV_FREQ_LIST;
  out[idx++] = freqBytes;
  for (uint8_t i = 0; i < safeFreqCount; ++i) {
    const uint16_t f = freqMHz[i];
    out[idx++] = static_cast<uint8_t>(f & 0xFFU);
    out[idx++] = static_cast<uint8_t>((f >> 8) & 0xFFU);
  }

  out[idx++] = TLV_NODE_STATUS;
  out[idx++] = NODE_STATUS_LEN;
  out[idx++] = statusFlags;
  out[idx++] = static_cast<uint8_t>(lastUartAgeS & 0xFFU);
  out[idx++] = static_cast<uint8_t>((lastUartAgeS >> 8) & 0xFFU);

  const uint16_t crc = crc16_ccitt_false(out, idx);
  out[idx++] = static_cast<uint8_t>(crc & 0xFFU);
  out[idx++] = static_cast<uint8_t>((crc >> 8) & 0xFFU);

  return idx;
}
