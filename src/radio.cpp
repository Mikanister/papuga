#include "radio.h"

#include <Arduino.h>
#include <SPI.h>
#include <SX126XLT.h>

#include "config.h"
#include "log.h"

namespace {

int16_t gLastRssi = 0;
int8_t gLastSnr = 0;
bool gRadioReady = false;
uint8_t gLastCode = 0;
SX126XLT gLt;

constexpr uint32_t RADIO_BUSY_TIMEOUT_MS = 10UL;
constexpr uint32_t RADIO_RX_CONT_TIMEOUT = 0x00FFFFFFUL;

uint8_t mapBwHzToLib(uint32_t bwHz) {
  switch (bwHz) {
    case 125000UL:
      return LORA_BW_125;
    case 250000UL:
      return LORA_BW_250;
    case 500000UL:
      return LORA_BW_500;
    default:
      return LORA_BW_125;
  }
}

uint8_t mapSfToLib(uint8_t sf) {
  switch (sf) {
    case 5:
      return LORA_SF5;
    case 6:
      return LORA_SF6;
    case 7:
      return LORA_SF7;
    case 8:
      return LORA_SF8;
    case 9:
      return LORA_SF9;
    case 10:
      return LORA_SF10;
    case 11:
      return LORA_SF11;
    case 12:
      return LORA_SF12;
    default:
      return LORA_SF9;
  }
}

bool mapCrToLib(uint8_t cr, uint8_t* libCr) {
  switch (cr) {
    case 5:
      *libCr = LORA_CR_4_5;
      return true;
    case 6:
      *libCr = LORA_CR_4_6;
      return true;
    case 7:
      *libCr = LORA_CR_4_7;
      return true;
    case 8:
      *libCr = LORA_CR_4_8;
      return true;
    default:
      return false;
  }
}

bool waitBusyLow(uint32_t timeoutMs) {
  const uint32_t t0 = millis();
  while (digitalRead(CFG_PIN_BUSY) == HIGH) {
    if ((millis() - t0) >= timeoutMs) {
      return false;
    }
  }
  return true;
}

uint8_t readStatusByte() {
  SPI.beginTransaction(SPISettings(1000000UL, MSBFIRST, SPI_MODE0));
  digitalWrite(CFG_PIN_NSS, LOW);
  SPI.transfer(0xC0);  // SX126x GetStatus opcode.
  const uint8_t status = SPI.transfer(0x00);
  digitalWrite(CFG_PIN_NSS, HIGH);
  SPI.endTransaction();
  return status;
}

bool radioSelfTest(uint8_t* outVal) {
  if (!waitBusyLow(RADIO_BUSY_TIMEOUT_MS)) {
    return false;
  }

  const uint8_t v1 = readStatusByte();
  delay(2);
  const uint8_t v2 = readStatusByte();
  delay(2);
  const uint8_t v3 = readStatusByte();

  const bool stable = (v1 == v2) && (v2 == v3);
  const bool valid = (v1 != 0x00) && (v1 != 0xFF);

  if (stable && valid) {
    *outVal = v1;
    return true;
  }
  return false;
}

bool applyLoRaProfile() {
  const uint8_t sfLib = mapSfToLib(LORA_SF);
  const uint8_t bwLib = mapBwHzToLib(LORA_BW_HZ);

  uint8_t crLib = 0;
  uint8_t crApplied = LORA_CR;
  bool useFallback = false;

  if (!mapCrToLib(LORA_CR, &crLib)) {
    if (!mapCrToLib(LORA_CR_FALLBACK, &crLib)) {
      return false;
    }
    crApplied = LORA_CR_FALLBACK;
    useFallback = true;
  }

  gLt.setupLoRa(LORA_FREQ_HZ, 0, sfLib, bwLib, crLib, LDRO_AUTO);
  gLt.setTxParams(LORA_TX_POWER_DBM, RADIO_RAMP_40_US);

  logEvent3("RPHY", LORA_SF, static_cast<int32_t>(LORA_BW_HZ));
  logEvent2("RCR", crApplied);
  if (useFallback) {
    logEvent2("RCRF", LORA_CR_FALLBACK);
  }
  logEvent2("RPWR", LORA_TX_POWER_DBM);

  return true;
}

}  // namespace

bool radioInit() {
  pinMode(CFG_PIN_NSS, OUTPUT);
  digitalWrite(CFG_PIN_NSS, HIGH);

  pinMode(CFG_PIN_RESET, OUTPUT);
  digitalWrite(CFG_PIN_RESET, LOW);
  delay(2);
  digitalWrite(CFG_PIN_RESET, HIGH);

  pinMode(CFG_PIN_BUSY, INPUT);
  pinMode(CFG_PIN_DIO1, INPUT);

  pinMode(CFG_PIN_TXEN, OUTPUT);
  pinMode(CFG_PIN_RXEN, OUTPUT);
  digitalWrite(CFG_PIN_TXEN, LOW);
  digitalWrite(CFG_PIN_RXEN, HIGH);

  SPI.setSCLK(CFG_PIN_SPI_SCK);
  SPI.setMISO(CFG_PIN_SPI_MISO);
  SPI.setMOSI(CFG_PIN_SPI_MOSI);
  SPI.begin();

  uint8_t probeVal = 0;
  if (radioSelfTest(&probeVal)) {
    logEvent2("RPROBE OK", probeVal);
  } else {
    logEvent("RPROBE FAIL");
    gRadioReady = false;
    gLastCode = 1;
    logEvent2("RINIT FAIL", 1);
    return false;
  }

  const bool beginOk = gLt.begin(
      CFG_PIN_NSS, CFG_PIN_RESET, CFG_PIN_BUSY, CFG_PIN_DIO1, CFG_PIN_RXEN, CFG_PIN_TXEN, DEVICE_SX1268);
  if (!beginOk) {
    gRadioReady = false;
    gLastCode = 2;
    logEvent2("RINIT FAIL", 2);
    return false;
  }

  if (!applyLoRaProfile()) {
    gRadioReady = false;
    gLastCode = 3;
    logEvent2("RINIT FAIL", 3);
    return false;
  }

  gRadioReady = true;
  gLastCode = 0;
  logEvent("RINIT OK");

  if (RADIO_RX_CONTINUOUS) {
    if (!radioStartRx()) {
      gRadioReady = false;
      return false;
    }
  }

  return gRadioReady;
}

bool radioSend(const uint8_t* data, uint8_t len) {
  if (!gRadioReady) {
    gLastCode = 10;
    return false;
  }

  const uint8_t sent = gLt.transmit(const_cast<uint8_t*>(data), len, 3000UL, LORA_TX_POWER_DBM, WAIT_TX);
  if (sent == len) {
    gLastCode = 0;
    return true;
  }

  const uint16_t irq = gLt.readIrqStatus();
  if ((irq & IRQ_RX_TX_TIMEOUT) != 0U) {
    gLastCode = 11;
  } else {
    gLastCode = 12;
  }
  return false;
}

bool radioStartRx() {
  if (!gRadioReady) {
    gLastCode = 20;
    logEvent("RRX FAIL");
    return false;
  }

  gLt.clearIrqStatus(IRQ_RADIO_ALL);
  gLt.setRx(RADIO_RX_CONT_TIMEOUT);
  gLastCode = 0;
  logEvent("RRX ON");
  return true;
}

bool radioIsIdle() {
  if (!gRadioReady) {
    return false;
  }
  return digitalRead(CFG_PIN_BUSY) == LOW;
}

uint8_t radioRead(uint8_t* out, uint8_t maxLen) {
  if (!gRadioReady) {
    gLastCode = 30;
    return 0;
  }

  const uint16_t irq = gLt.readIrqStatus();
  const uint16_t rxErrMask = IRQ_HEADER_ERROR | IRQ_CRC_ERROR | IRQ_RX_TX_TIMEOUT;

  if ((irq & rxErrMask) != 0U) {
    gLastCode = 31;
    gLt.clearIrqStatus(IRQ_RADIO_ALL);
    if (RADIO_RX_CONTINUOUS) {
      gLt.setRx(RADIO_RX_CONT_TIMEOUT);
    }
    return 0;
  }

  if ((irq & IRQ_RX_DONE) == 0U) {
    return 0;
  }

  const uint8_t rxLen = gLt.readPacket(out, maxLen);
  gLastRssi = gLt.readPacketRSSI();
  gLastSnr = gLt.readPacketSNR();
  gLt.clearIrqStatus(IRQ_RADIO_ALL);
  if (RADIO_RX_CONTINUOUS) {
    gLt.setRx(RADIO_RX_CONT_TIMEOUT);
  }
  gLastCode = 0;
  return rxLen;
}

uint8_t radioLastCode() {
  return gLastCode;
}

int16_t radioLastRssi() {
  return gLastRssi;
}

int8_t radioLastSnr() {
  return gLastSnr;
}
