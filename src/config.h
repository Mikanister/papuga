#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>  // PAx/PBx/PCx pin constants and fixed-width integer types.

// ===== Build switches =====
#ifndef LOG_ENABLED
#define LOG_ENABLED 1
#endif
constexpr bool RADIO_FRAME_SELFTEST = false;
constexpr bool RADIO_TEST_TX = false;
constexpr bool RADIO_TEST_RX = false;
constexpr bool RADIO_TEST_BIDIR = false;
constexpr bool ENABLE_HEARTBEAT = false;
constexpr uint32_t HEARTBEAT_PERIOD_MS = 5000UL;

// ===== Node identity =====
// Change NODE_ID per physical node before flashing (e.g. 1, 2, 3...).
// Do not clone firmware to multiple nodes with the same NODE_ID.
constexpr uint8_t NODE_ID = 1;
constexpr bool IS_GATEWAY = false;
constexpr uint8_t NET_ID = 1;

// ===== Pins =====
constexpr uint8_t CFG_PIN_SPI_SCK = PA5;
constexpr uint8_t CFG_PIN_SPI_MISO = PA6;
constexpr uint8_t CFG_PIN_SPI_MOSI = PA7;

// NSS is mandatory even with a single SPI device.
constexpr uint8_t CFG_PIN_NSS = PA4;
constexpr uint8_t CFG_PIN_RESET = PB10;
constexpr uint8_t CFG_PIN_BUSY = PB1;
constexpr uint8_t CFG_PIN_DIO1 = PB0;

constexpr uint8_t CFG_PIN_TXEN = PB11;
constexpr uint8_t CFG_PIN_RXEN = PB12;

// Blue Pill LED on PC13 is active-low.
constexpr uint8_t CFG_PIN_LED = PC13;
constexpr uint8_t PIN_BATT_ADC = PA0;

// ===== PHY =====
// Reliability profile defaults.
constexpr uint32_t LORA_FREQ_HZ = 433000000UL;
constexpr uint8_t LORA_SF = 9;
constexpr uint32_t LORA_BW_HZ = 125000UL;
// Coding rate map used in this project:
// 5 -> 4/5, 6 -> 4/6, 7 -> 4/7, 8 -> 4/8.
constexpr uint8_t LORA_CR = 6;           // 4/6
constexpr uint8_t LORA_CR_FALLBACK = 5;  // 4/5
constexpr int8_t LORA_TX_POWER_DBM = 2;
constexpr bool RADIO_RX_CONTINUOUS = true;

// ===== Mesh =====
// Mesh constants v1.5
constexpr uint8_t BEACON_TTL_HOPS = 3;
constexpr uint32_t GW_TIMEOUT_MS = 120000UL;
constexpr uint8_t DATA_TTL = 8;
constexpr uint8_t DATA_TTL_EMERG = 12;
constexpr uint16_t DEDUP_N = 128;
constexpr uint32_t BACKOFF_MIN_MS = 50UL;
constexpr uint32_t BACKOFF_MAX_MS = 300UL;
constexpr uint8_t MAX_FORWARDS_PER_WINDOW = 10;
constexpr uint32_t WINDOW_MS = 10000UL;

// ===== UART =====
constexpr uint32_t UART_BAUD = 115200UL;
constexpr uint8_t UART_LINE_MAX = 64;
constexpr uint8_t UART_MAX_BYTES_PER_TICK = 64;
constexpr uint8_t MAX_FREQS = 5;
// UART format: ASCII list of frequencies in MHz, terminated by '\n'.

// ===== Battery ADC =====
constexpr uint32_t BATT_LOG_PERIOD_MS = 10000UL;
constexpr uint32_t REPORT_STATUS_PERIOD_MS = 5000UL;

#endif  // CONFIG_H
