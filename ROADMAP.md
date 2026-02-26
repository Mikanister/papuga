# Project Roadmap

## EPIC 0 — Infrastructure (Done)

Goal: Project skeleton and discipline.

Scope:
- Modular `src/` structure
- Thin `.ino` wrapper
- `appTick(millis())` timebase
- Logging layer
- Board bring-up (LED, `boot_id`)

Status: Completed

## EPIC 1 — Radio + PHY + Frame Math (Done)

Goal: Stable LoRa PHY and deterministic frame format.

Scope:
- SX126x driver integration
- Fixed PHY profile (SF/BW/CR/frequency)
- CRC16 (CCITT-FALSE)
- Frame builder/parser
- Radio self-test
- TX/RX validation bench

Status: Completed

## EPIC 2 — Uplink Data Path (Done)

Goal: Real data flow from RPi -> LoRa.

Scope:
- Non-blocking UART ingest
- Frequency parser (up to `MAX_FREQS`)
- Battery ADC (`PA0`)
- REPORT frame (TLV: `FREQ_LIST` + `NODE_STATUS`)
- `DST_ID` support
- TX queue (static, no `malloc`)
- Random backoff
- Status refresh + timeout logic
- Flood protection hardening (UART budget per tick)

Status: Completed + hardened

## EPIC 3 — Mesh Layer v1 (Next)

Goal: Controlled multi-hop forwarding (bounded flooding).

Scope:
- Dedup cache (`SRC_ID` + `MSG_ID`)
- TTL decrement / HOPS increment
- Forward rate limiting (window-based)
- RX -> mesh handler wiring
- Forward telemetry counters

Status: Planned

## EPIC 4 — Gateway Beacons & Selection

Goal: Dedicated gateway nodes with Starlink.

Scope:
- `GW_BEACON` message type (`NO_RELAY`)
- Gateway freshness tracking
- Best + backup gateway selection
- REPORT `DST_ID` routing to selected gateway

Status: Planned

## EPIC 5 — Gateway Internet Forwarder

Goal: Gateway forwards received REPORT to internet uplink.

Scope:
- RX -> USB/UART bridge
- Line-based export protocol
- Optional buffering

Status: Planned

## EPIC 6 — Downlink v1 (Commands & Config)

Goal: Basic remote control capability.

Scope:
- `CMD` / `CONFIG` message types
- ACK support (minimal)
- Idempotent command handling
- Config versioning

Status: Parked

## EPIC 7 — Field Hardening & Observability

Goal: Stability in real-world deployment.

Scope:
- Runtime counters
- Watchdog policy
- Diagnostic mode
- Load testing
- Storm resistance tuning

Status: Planned

## EPIC 8 — Security-lite (Optional)

Goal: Basic injection resistance.

Scope:
- Auth tag (future)
- Strict `NET_ID` filtering
- Drop malformed packets early

Status: Optional

## System Philosophy

- No dynamic memory
- No blocking logic
- Deterministic scheduling
- Bounded flooding instead of complex routing
- Expandable frame format (TLV)
- Gateway nodes separated from scanner nodes
