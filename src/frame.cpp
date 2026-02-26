#include "frame.h"

#include "board.h"
#include "config.h"
#include "crc16.h"

namespace {

constexpr uint8_t PING_TYPE = 0x01U;
constexpr uint8_t SCANNER_DST_ID = 0xFFU;
constexpr uint8_t PING_LEN = PING_FRAME_LEN;
constexpr uint8_t HEADER_LEN = 7U;
constexpr uint8_t CRC_LEN = 2U;
constexpr uint8_t NODE_STATUS_LEN = 3U;

}  // namespace

void buildPingFrame(uint16_t seq, uint8_t out[PING_FRAME_LEN]) {
  out[0] = NET_ID;
  out[1] = NODE_ID;
  out[2] = SCANNER_DST_ID;
  out[3] = boardBootId();
  out[4] = PING_TYPE;
  out[5] = static_cast<uint8_t>(seq & 0xFFU);
  out[6] = static_cast<uint8_t>((seq >> 8) & 0xFFU);

  const uint16_t crc = crc16_ccitt_false(out, HEADER_LEN);
  out[7] = static_cast<uint8_t>(crc & 0xFFU);
  out[8] = static_cast<uint8_t>((crc >> 8) & 0xFFU);
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

  const uint16_t crcCalc = crc16_ccitt_false(buf, HEADER_LEN);
  const uint16_t crcIn = static_cast<uint16_t>(buf[7]) |
                         static_cast<uint16_t>(static_cast<uint16_t>(buf[8]) << 8);
  if (crcCalc != crcIn) {
    errCode = 3U;
    return false;
  }

  seqOut = static_cast<uint16_t>(buf[5]) |
           static_cast<uint16_t>(static_cast<uint16_t>(buf[6]) << 8);
  srcOut = buf[1];
  bootOut = buf[3];
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

  out[0] = NET_ID;
  out[1] = NODE_ID;
  out[2] = dstId;
  out[3] = boardBootId();
  out[4] = REPORT_TYPE;
  out[5] = static_cast<uint8_t>(seq & 0xFFU);
  out[6] = static_cast<uint8_t>((seq >> 8) & 0xFFU);

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
