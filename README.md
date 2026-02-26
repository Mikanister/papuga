# LoRa Mesh Node (STM32)

## Target Hardware

- Blue Pill `STM32F103C8`

## Core

- STMicroelectronics STM32duino

## Current Status

- EPIC 0, 1, 2: completed (`EPIC 2` hardened)
- Next target: EPIC 3 (Mesh Layer v1)
- Full plan: see `ROADMAP.md`

## Project Structure

```text
E22_transiever/
|- E22_transiever.ino
|- README.md
`- src/
   |- app.cpp
   |- app.h
   |- board.cpp
   |- board.h
   |- config.h
   |- log.h
   |- radio.cpp
   `- radio.h
```

## Build Toggles

Edit `src/config.h`:

- Logging on/off: set `LOG_ENABLED` to `1` or `0`.
- Heartbeat on/off: set `ENABLE_HEARTBEAT` to `true` or `false`.
- Node identity and role: update `NODE_ID` and `IS_GATEWAY`.

## Radio Wiring

All radio-related pins are taken from `src/config.h`.

- `NSS`: SPI chip select for SX126x commands.
- `BUSY`: module busy line; host waits until low before SPI command exchange.
- `DIO1`: radio interrupt line for future RX/TX done events.
- `TXEN` / `RXEN`: RF switch control lines (TX path / RX path selection).

## Bench Test Quick Start

- TX node (`src/config.h`): `RADIO_TEST_TX=true`, `RADIO_TEST_RX=false`, `RADIO_TEST_BIDIR=false`.
- RX node (`src/config.h`): `RADIO_TEST_TX=false`, `RADIO_TEST_RX=true`, `RADIO_TEST_BIDIR=false`.
- Single-node mixed test: set only `RADIO_TEST_BIDIR=true` (TX every 3s and RX in parallel).
- Check wiring first: `NSS`, `BUSY`, `DIO1`, `TXEN`, `RXEN` must match `src/config.h` pin map.
- Expected logs: TX side `TXOK <seq>`; RX side `RXOK <seq> <rssi>` (+ `RSNR <snr>`); errors show as `TXFAIL <code>` or `RXBAD <code>`.

## Notes

- USB CDC will be used for debug later.

## Python Protocol Tests

- Install test dependency: `python -m pip install -r requirements-test.txt`
- Run tests: `python -m pytest -q`
- Coverage focus:
  - CRC16 (CCITT-FALSE) vector
  - PING frame build/parse checks
  - REPORT TLV layout + CRC
  - UART frequency parser edge cases
  - Status flags / UART timeout behavior
