#include "app.h"

#include "board.h"
#include "config.h"
#include "frame.h"
#include "log.h"
#include "radio.h"
#include "uart.h"

namespace {

AppState gState;
bool gTimebaseReady = false;

uint32_t gLastHeartbeatMs = 0;
uint32_t gLastBattLogMs = 0;
uint32_t gLastBeaconSlotMs = 0;
uint32_t gLastWindowTickMs = 0;
uint32_t gLastPingEnqueueMs = 0;
uint16_t gTxSeq = 0;
uint16_t gReportSeq = 0;
uint32_t gLastParsedUartMs = 0;
uint32_t gNextTxAtMs = 0;
uint32_t gLastStatusReportMs = 0;
uint8_t gLastReportedFlags = 0xFFU;

uint16_t gParsedFreqMHz[MAX_FREQS] = {0};
uint8_t gParsedFreqCount = 0;
uint8_t SDR_OK = 0;

constexpr uint32_t BEACON_SLOT_PERIOD_MS = 1000UL;
constexpr uint32_t WINDOW_TICK_PERIOD_MS = 1000UL;
constexpr uint8_t TX_QUEUE_CAPACITY = 4U;
constexpr uint8_t TX_FRAME_MAX = 64U;
constexpr uint32_t RPI_UART_FRESH_MS = 15000UL;
constexpr uint16_t LOW_BATT_THRESHOLD_MV = 3300U;
constexpr uint8_t STATUS_RPI_OK_BIT = 0;
constexpr uint8_t STATUS_SDR_OK_BIT = 1;
constexpr uint8_t STATUS_UART_VALID_BIT = 2;
constexpr uint8_t STATUS_LOW_BATT_BIT = 3;
constexpr uint8_t SCANNER_DST_ID = 0xFFU;
constexpr bool RADIO_TEST_TX_ACTIVE = RADIO_TEST_TX || RADIO_TEST_BIDIR;
constexpr bool RADIO_TEST_RX_ACTIVE = RADIO_TEST_RX || RADIO_TEST_BIDIR;

struct TxItem {
  uint8_t len;
  uint8_t data[TX_FRAME_MAX];
};

TxItem gTxQueue[TX_QUEUE_CAPACITY] = {};
uint8_t gTxHead = 0;
uint8_t gTxTail = 0;
uint8_t gTxCount = 0;

bool isSep(char ch) {
  return (ch == ',') || (ch == ' ') || (ch == '\t');
}

bool timeReached(uint32_t nowMs, uint32_t deadlineMs) {
  return static_cast<int32_t>(nowMs - deadlineMs) >= 0;
}

uint32_t randomBackoffMs() {
  if (BACKOFF_MAX_MS <= BACKOFF_MIN_MS) {
    return BACKOFF_MIN_MS;
  }
  return static_cast<uint32_t>(random(static_cast<long>(BACKOFF_MIN_MS),
                                      static_cast<long>(BACKOFF_MAX_MS + 1UL)));
}

bool txQueuePush(const uint8_t* data, uint8_t len) {
  if ((data == nullptr) || (len == 0U) || (len > TX_FRAME_MAX)) {
    return false;
  }
  if (gTxCount >= TX_QUEUE_CAPACITY) {
    // Full queue policy: drop newest frame.
    logEvent("QSAT");
    return false;
  }

  gTxQueue[gTxTail].len = len;
  for (uint8_t i = 0; i < len; ++i) {
    gTxQueue[gTxTail].data[i] = data[i];
  }

  gTxTail = static_cast<uint8_t>((gTxTail + 1U) % TX_QUEUE_CAPACITY);
  ++gTxCount;
  logEvent2("QADD", gTxCount);
  return true;
}

const TxItem* txQueueFront() {
  if (gTxCount == 0U) {
    return nullptr;
  }
  return &gTxQueue[gTxHead];
}

void txQueuePop() {
  if (gTxCount == 0U) {
    return;
  }
  gTxHead = static_cast<uint8_t>((gTxHead + 1U) % TX_QUEUE_CAPACITY);
  --gTxCount;
}

uint16_t frameSeq(const uint8_t* frame, uint8_t len) {
  if ((frame == nullptr) || (len < 7U)) {
    return 0U;
  }
  return static_cast<uint16_t>(frame[5]) |
         static_cast<uint16_t>(static_cast<uint16_t>(frame[6]) << 8);
}

void runTxScheduler(uint32_t nowMs) {
  const TxItem* item = txQueueFront();
  if (item == nullptr) {
    gNextTxAtMs = 0U;
    return;
  }

  if (gNextTxAtMs == 0U) {
    gNextTxAtMs = nowMs + randomBackoffMs();
    return;
  }

  if (!timeReached(nowMs, gNextTxAtMs)) {
    return;
  }

  if (!radioIsIdle()) {
    return;
  }

  const uint16_t seq = frameSeq(item->data, item->len);
  if (radioSend(item->data, item->len)) {
    logEvent2("TXOK", seq);
    txQueuePop();
  } else {
    logEvent2("TXFAIL", radioLastCode());
  }

  (void)radioStartRx();
  gNextTxAtMs = nowMs + randomBackoffMs();
}

void pushFreqIfValid(uint32_t value, uint16_t outMHz[MAX_FREQS], uint8_t& outCount) {
  if ((value == 0UL) || (value > 65535UL)) {
    return;
  }
  if (outCount >= MAX_FREQS) {
    return;
  }
  outMHz[outCount] = static_cast<uint16_t>(value);
  ++outCount;
}

uint8_t parseFreqLineMHz(const char* line, uint16_t outMHz[MAX_FREQS]) {
  uint8_t outCount = 0;
  uint32_t value = 0;
  bool hasDigits = false;
  bool tokenOverflow = false;

  for (uint16_t i = 0; ; ++i) {
    const char ch = line[i];
    const bool isEnd = (ch == '\0');

    if (!isEnd && (ch >= '0') && (ch <= '9')) {
      hasDigits = true;
      if (!tokenOverflow) {
        value = (value * 10UL) + static_cast<uint32_t>(ch - '0');
        if (value > 65535UL) {
          tokenOverflow = true;
        }
      }
      continue;
    }

    if (isEnd || isSep(ch)) {
      if (hasDigits && !tokenOverflow) {
        pushFreqIfValid(value, outMHz, outCount);
      }
      value = 0;
      hasDigits = false;
      tokenOverflow = false;
      if (isEnd) {
        break;
      }
      continue;
    }

    // Any non-digit/non-separator character ends current token safely.
    if (hasDigits && !tokenOverflow) {
      pushFreqIfValid(value, outMHz, outCount);
    }
    value = 0;
    hasDigits = false;
    tokenOverflow = false;
  }

  return outCount;
}

uint16_t calcLastUartAgeS(uint32_t nowMs, bool hasUart, uint32_t uartTsMs) {
  if (!hasUart) {
    return 0xFFFFU;
  }
  const uint32_t ageMs = nowMs - uartTsMs;
  const uint32_t ageS = ageMs / 1000UL;
  if (ageS > 65535UL) {
    return 0xFFFFU;
  }
  return static_cast<uint16_t>(ageS);
}

uint8_t buildStatusFlags(bool hasUart, uint16_t lastUartAgeS, uint16_t battMv) {
  uint8_t flags = 0U;
  if (hasUart && (lastUartAgeS != 0xFFFFU) &&
      (static_cast<uint32_t>(lastUartAgeS) * 1000UL <= RPI_UART_FRESH_MS)) {
    flags |= static_cast<uint8_t>(1U << STATUS_RPI_OK_BIT);
  }
  if (SDR_OK != 0U) {
    flags |= static_cast<uint8_t>(1U << STATUS_SDR_OK_BIT);
  }
  if (hasUart) {
    flags |= static_cast<uint8_t>(1U << STATUS_UART_VALID_BIT);
  }
  if (battMv < LOW_BATT_THRESHOLD_MV) {
    flags |= static_cast<uint8_t>(1U << STATUS_LOW_BATT_BIT);
  }
  return flags;
}

void enqueueReport(uint32_t nowMs, bool forceReport) {
  const bool hasUart = uartHasValidLine();
  const uint16_t lastUartAgeS = calcLastUartAgeS(nowMs, hasUart, uartLastTimestampMs());
  const uint16_t battMv = battReadMv();
  const uint8_t statusFlags = buildStatusFlags(hasUart, lastUartAgeS, battMv);

  const bool flagsChanged = (statusFlags != gLastReportedFlags);
  const bool periodicDue =
      ((nowMs - gLastStatusReportMs) >= REPORT_STATUS_PERIOD_MS);

  if (!forceReport && !flagsChanged && !periodicDue) {
    return;
  }

  uint8_t reportBuf[64] = {0};
  const uint8_t reportLen = buildReportFrame(gReportSeq,
                                             SCANNER_DST_ID,
                                             gParsedFreqMHz,
                                             gParsedFreqCount,
                                             statusFlags,
                                             lastUartAgeS,
                                             reportBuf,
                                             sizeof(reportBuf));
  if (reportLen > 0U) {
    if (txQueuePush(reportBuf, reportLen)) {
      logEvent2("RPT", reportLen);
      ++gReportSeq;
      gLastStatusReportMs = nowMs;
      gLastReportedFlags = statusFlags;
    }
  }
}

void processLatestUartLine(uint32_t nowMs) {
  if (!uartHasValidLine()) {
    return;
  }

  const uint32_t tsMs = uartLastTimestampMs();
  if (tsMs == gLastParsedUartMs) {
    return;
  }
  gLastParsedUartMs = tsMs;

  uint16_t parsedMHz[MAX_FREQS] = {0};
  const uint8_t count = parseFreqLineMHz(uartLastValidLine(), parsedMHz);

  gParsedFreqCount = count;
  for (uint8_t i = 0; i < MAX_FREQS; ++i) {
    gParsedFreqMHz[i] = parsedMHz[i];
  }

  if (count > 0U) {
    SDR_OK = 1U;
    logEvent2("PFREQ", count);
  } else {
    SDR_OK = 0U;
    logEvent("PBAD");
  }
  enqueueReport(nowMs, true);
}

}  // namespace

void appInit() {
  gState.mode = NodeMode::Idle;
  gTimebaseReady = false;
  uartInit();
  const uint32_t seed = static_cast<uint32_t>(boardBootId()) ^
                        (static_cast<uint32_t>(battReadMv()) << 8) ^
                        micros();
  randomSeed(seed);

  if constexpr (RADIO_FRAME_SELFTEST) {
    uint8_t frame[PING_FRAME_LEN];
    buildPingFrame(1U, frame);

    uint16_t seqOut = 0U;
    uint8_t srcOut = 0U;
    uint8_t bootOut = 0U;
    uint8_t errCode = 0U;

    uint8_t frameHead8[8] = {0};
    for (uint8_t i = 0; i < 8U; ++i) {
      frameHead8[i] = frame[i];
    }
    logHex8("FHEX", frameHead8);
    const uint16_t frameCrc = static_cast<uint16_t>(frame[7]) |
                              static_cast<uint16_t>(static_cast<uint16_t>(frame[8]) << 8);
    logEvent2("FCRC", frameCrc);

    if (parsePingFrame(frame, PING_FRAME_LEN, seqOut, srcOut, bootOut, errCode)) {
      logEvent("FSELF OK");
    } else {
      logEvent2("FSELF FAIL", errCode);
    }
  }

  if constexpr (RADIO_TEST_TX_ACTIVE || RADIO_TEST_RX_ACTIVE) {
    (void)radioInit();
  }
}

void appTick(uint32_t nowMs) {
  uartPoll(nowMs);
  processLatestUartLine(nowMs);
  enqueueReport(nowMs, false);

  if (!gTimebaseReady) {
    gLastHeartbeatMs = nowMs;
    gLastBattLogMs = nowMs;
    gLastBeaconSlotMs = nowMs;
    gLastWindowTickMs = nowMs;
    gLastPingEnqueueMs = nowMs;
    gLastStatusReportMs = nowMs;
    gTimebaseReady = true;
  }

  if ((nowMs - gLastBeaconSlotMs) >= BEACON_SLOT_PERIOD_MS) {
    gLastBeaconSlotMs = nowMs;
    // Placeholder for beacon scheduling/jitter logic.
  }

  if ((nowMs - gLastWindowTickMs) >= WINDOW_TICK_PERIOD_MS) {
    gLastWindowTickMs = nowMs;
    // Placeholder for future rate-limit window maintenance.
  }

  if constexpr (RADIO_TEST_TX_ACTIVE) {
    if ((nowMs - gLastPingEnqueueMs) >= 3000UL) {
      gLastPingEnqueueMs = nowMs;

      uint8_t frame[PING_FRAME_LEN];
      buildPingFrame(gTxSeq, frame);
      if (txQueuePush(frame, PING_FRAME_LEN)) {
        ++gTxSeq;
      }
    }
  }

  if constexpr (RADIO_TEST_TX_ACTIVE || RADIO_TEST_RX_ACTIVE) {
    runTxScheduler(nowMs);
  }

  if constexpr (RADIO_TEST_RX_ACTIVE) {
    uint8_t rxBuf[32];
    const uint8_t rxLen = radioRead(rxBuf, sizeof(rxBuf));
    if (rxLen > 0U) {
      uint16_t seqOut = 0U;
      uint8_t srcOut = 0U;
      uint8_t bootOut = 0U;
      uint8_t errCode = 0U;

      if (parsePingFrame(rxBuf, rxLen, seqOut, srcOut, bootOut, errCode)) {
        logEvent3("RXOK", seqOut, radioLastRssi());
        logEvent2("RSNR", radioLastSnr());
        boardLedPulse(100);
      } else {
        logEvent2("RXBAD", errCode);
      }
    }
  }

#if LOG_ENABLED
  if ((nowMs - gLastBattLogMs) >= BATT_LOG_PERIOD_MS) {
    gLastBattLogMs = nowMs;
    logEvent2("BATT", battReadMv());
  }

  if (ENABLE_HEARTBEAT && ((nowMs - gLastHeartbeatMs) >= HEARTBEAT_PERIOD_MS)) {
    gLastHeartbeatMs = nowMs;
    logEvent2("ALIVE", NODE_ID);
  }
#endif
}
