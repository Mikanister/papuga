from __future__ import annotations

from dataclasses import dataclass
from typing import List, Optional


PING_TYPE = 0x01
REPORT_TYPE = 0x10
TLV_FREQ_LIST = 0x01
TLV_NODE_STATUS = 0x02
FRAME_FLAG_NO_RELAY = 0x01
PING_FRAME_LEN = 12
HEADER_LEN = 10
MAX_FREQS = 5


def crc16_ccitt_false(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc ^= (b << 8) & 0xFFFF
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


def build_ping_frame(net_id: int, src_id: int, dst_id: int, boot_id: int, seq: int) -> bytes:
    head = bytes(
        [
            net_id & 0xFF,
            src_id & 0xFF,
            dst_id & 0xFF,
            boot_id & 0xFF,
            PING_TYPE,
            seq & 0xFF,
            (seq >> 8) & 0xFF,
            8,   # TTL
            0,   # HOPS
            0,   # FLAGS
        ]
    )
    crc = crc16_ccitt_false(head)
    return head + bytes([crc & 0xFF, (crc >> 8) & 0xFF])


@dataclass
class PingParseResult:
    ok: bool
    err_code: int
    seq: int = 0
    src_id: int = 0
    boot_id: int = 0


def parse_ping_frame(buf: bytes, expected_net_id: int) -> PingParseResult:
    if len(buf) != PING_FRAME_LEN:
        return PingParseResult(ok=False, err_code=1)
    if buf[0] != (expected_net_id & 0xFF):
        return PingParseResult(ok=False, err_code=2)
    if buf[4] != PING_TYPE:
        return PingParseResult(ok=False, err_code=4)

    crc_calc = crc16_ccitt_false(buf[:HEADER_LEN])
    crc_in = buf[-2] | (buf[-1] << 8)
    if crc_calc != crc_in:
        return PingParseResult(ok=False, err_code=3)

    seq = buf[5] | (buf[6] << 8)
    return PingParseResult(ok=True, err_code=0, seq=seq, src_id=buf[1], boot_id=buf[3])


def parse_freq_line_mhz(line: str, max_freqs: int = MAX_FREQS) -> List[int]:
    out: List[int] = []
    value = 0
    has_digits = False
    token_overflow = False

    def push() -> None:
        nonlocal value, has_digits, token_overflow
        if has_digits and (not token_overflow) and value != 0 and value <= 0xFFFF and len(out) < max_freqs:
            out.append(value)
        value = 0
        has_digits = False
        token_overflow = False

    for ch in line + "\0":
        if "0" <= ch <= "9":
            has_digits = True
            if not token_overflow:
                value = value * 10 + (ord(ch) - ord("0"))
                if value > 0xFFFF:
                    token_overflow = True
            continue

        if ch in {",", " ", "\t", "\0"}:
            push()
            if ch == "\0":
                break
            continue

        push()

    return out


def calc_last_uart_age_s(now_ms: int, has_uart: bool, uart_ts_ms: int) -> int:
    if not has_uart:
        return 0xFFFF
    age_s = max(0, now_ms - uart_ts_ms) // 1000
    return min(age_s, 0xFFFF)


def build_status_flags(
    *,
    has_uart: bool,
    last_uart_age_s: int,
    batt_mv: int,
    sdr_ok: bool,
    rpi_uart_fresh_ms: int = 15000,
    low_batt_threshold_mv: int = 3300,
) -> int:
    flags = 0
    if has_uart and last_uart_age_s != 0xFFFF and (last_uart_age_s * 1000) <= rpi_uart_fresh_ms:
        flags |= 1 << 0  # RPi_OK
    if sdr_ok:
        flags |= 1 << 1  # SDR_OK
    if has_uart:
        flags |= 1 << 2  # UART_VALID
    if batt_mv < low_batt_threshold_mv:
        flags |= 1 << 3  # LOW_BATT
    return flags


def build_report_frame(
    *,
    net_id: int,
    src_id: int,
    dst_id: int,
    boot_id: int,
    seq: int,
    freq_mhz: List[int],
    status_flags: int,
    last_uart_age_s: int,
    out_max: int = 64,
) -> Optional[bytes]:
    safe_freqs = [f & 0xFFFF for f in freq_mhz[:MAX_FREQS]]
    freq_bytes = len(safe_freqs) * 2

    payload = bytearray()
    payload += bytes([TLV_FREQ_LIST, freq_bytes])
    for f in safe_freqs:
        payload += bytes([f & 0xFF, (f >> 8) & 0xFF])

    payload += bytes([TLV_NODE_STATUS, 3, status_flags & 0xFF, last_uart_age_s & 0xFF, (last_uart_age_s >> 8) & 0xFF])

    head = bytes(
        [
            net_id & 0xFF,
            src_id & 0xFF,
            dst_id & 0xFF,
            boot_id & 0xFF,
            REPORT_TYPE,
            seq & 0xFF,
            (seq >> 8) & 0xFF,
            8,   # TTL
            0,   # HOPS
            0,   # FLAGS
        ]
    )
    full_no_crc = head + payload
    if len(full_no_crc) + 2 > out_max:
        return None

    crc = crc16_ccitt_false(full_no_crc)
    return full_no_crc + bytes([crc & 0xFF, (crc >> 8) & 0xFF])


def frame_crc_ok(frame: bytes) -> bool:
    if len(frame) < HEADER_LEN + 2:
        return False
    crc_calc = crc16_ccitt_false(frame[:-2])
    crc_in = frame[-2] | (frame[-1] << 8)
    return crc_calc == crc_in


def frame_get_ttl(frame: bytes) -> int:
    if len(frame) < HEADER_LEN + 2:
        return 0
    return frame[7]


def frame_is_no_relay(frame: bytes) -> bool:
    if len(frame) < HEADER_LEN + 2:
        return True
    return (frame[9] & FRAME_FLAG_NO_RELAY) != 0


def frame_dec_ttl_inc_hops_recrc(frame: bytes) -> Optional[bytes]:
    if len(frame) < HEADER_LEN + 2:
        return None
    if not frame_crc_ok(frame):
        return None
    ttl = frame[7]
    if ttl == 0:
        return None

    out = bytearray(frame)
    out[7] = (out[7] - 1) & 0xFF
    out[8] = (out[8] + 1) & 0xFF
    crc = crc16_ccitt_false(bytes(out[:-2]))
    out[-2] = crc & 0xFF
    out[-1] = (crc >> 8) & 0xFF
    return bytes(out)


def mesh_should_forward(frame: bytes, *, dedup_seen: bool, rate_allow: bool) -> bool:
    if not frame_crc_ok(frame):
        return False
    if dedup_seen:
        return False
    if frame_get_ttl(frame) == 0:
        return False
    if frame_is_no_relay(frame):
        return False
    if not rate_allow:
        return False
    return True


@dataclass
class ForwardQueue:
    capacity: int
    size: int = 0
    saturated_events: int = 0

    def push(self) -> bool:
        # Firmware policy: drop newest when full.
        if self.size >= self.capacity:
            self.saturated_events += 1
            return False
        self.size += 1
        return True


@dataclass
class ForwardWindowLimiter:
    window_ms: int
    max_forwards_per_window: int
    window_start_ms: int = 0
    count_in_window: int = 0

    def allow(self, now_ms: int) -> bool:
        if now_ms - self.window_start_ms >= self.window_ms:
            self.window_start_ms = now_ms
            self.count_in_window = 0
        if self.count_in_window < self.max_forwards_per_window:
            return True
        return False

    def consume(self) -> None:
        self.count_in_window += 1
