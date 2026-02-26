from tools.protocol_model import (
    FRAME_FLAG_NO_RELAY,
    ForwardQueue,
    ForwardWindowLimiter,
    HEADER_LEN,
    PING_FRAME_LEN,
    PING_TYPE,
    REPORT_TYPE,
    TLV_FREQ_LIST,
    TLV_NODE_STATUS,
    build_ping_frame,
    build_report_frame,
    build_status_flags,
    calc_last_uart_age_s,
    crc16_ccitt_false,
    frame_crc_ok,
    frame_dec_ttl_inc_hops_recrc,
    parse_freq_line_mhz,
    parse_ping_frame,
    mesh_should_forward,
)


def test_crc16_ccitt_false_known_vector() -> None:
    assert crc16_ccitt_false(b"123456789") == 0x29B1


def test_ping_build_and_parse_roundtrip() -> None:
    frame = build_ping_frame(net_id=1, src_id=1, dst_id=0xFF, boot_id=0xAB, seq=0x1234)
    assert len(frame) == PING_FRAME_LEN
    assert frame[4] == PING_TYPE
    assert frame[2] == 0xFF

    parsed = parse_ping_frame(frame, expected_net_id=1)
    assert parsed.ok is True
    assert parsed.err_code == 0
    assert parsed.seq == 0x1234
    assert parsed.src_id == 1
    assert parsed.boot_id == 0xAB


def test_ping_parse_rejects_bad_type_and_bad_crc() -> None:
    frame = bytearray(build_ping_frame(net_id=1, src_id=2, dst_id=0xFF, boot_id=3, seq=7))
    frame[4] = 0xEE
    bad_type = parse_ping_frame(bytes(frame), expected_net_id=1)
    assert bad_type.ok is False
    assert bad_type.err_code == 4

    frame = bytearray(build_ping_frame(net_id=1, src_id=2, dst_id=0xFF, boot_id=3, seq=7))
    frame[7] ^= 0x01
    bad_crc = parse_ping_frame(bytes(frame), expected_net_id=1)
    assert bad_crc.ok is False
    assert bad_crc.err_code == 3


def test_freq_parser_commas_spaces_tabs_limits_and_zero() -> None:
    freq = parse_freq_line_mhz("433, 434 0\t435 436 437 438 99999")
    assert freq == [433, 434, 435, 436, 437]


def test_uart_age_and_status_flags_timeout_drop_rpi_ok() -> None:
    age_now = calc_last_uart_age_s(now_ms=1000, has_uart=True, uart_ts_ms=0)
    flags_now = build_status_flags(has_uart=True, last_uart_age_s=age_now, batt_mv=3600, sdr_ok=True)
    assert (flags_now & (1 << 0)) != 0  # RPi_OK
    assert (flags_now & (1 << 1)) != 0  # SDR_OK
    assert (flags_now & (1 << 2)) != 0  # UART_VALID
    assert (flags_now & (1 << 3)) == 0  # LOW_BATT

    age_2min = calc_last_uart_age_s(now_ms=120000, has_uart=True, uart_ts_ms=0)
    flags_2min = build_status_flags(has_uart=True, last_uart_age_s=age_2min, batt_mv=3200, sdr_ok=True)
    assert (flags_2min & (1 << 0)) == 0  # RPi_OK dropped
    assert (flags_2min & (1 << 1)) != 0  # SDR_OK still set
    assert (flags_2min & (1 << 2)) != 0  # UART_VALID still set (last valid snapshot exists)
    assert (flags_2min & (1 << 3)) != 0  # LOW_BATT


def test_report_tlv_layout_len_and_crc() -> None:
    frame = build_report_frame(
        net_id=1,
        src_id=1,
        dst_id=0xFF,
        boot_id=0x42,
        seq=9,
        freq_mhz=[433, 434, 435],
        status_flags=0b00000110,
        last_uart_age_s=5,
    )
    assert frame is not None
    assert frame[4] == REPORT_TYPE

    payload = frame[HEADER_LEN:-2]
    assert payload[0] == TLV_FREQ_LIST
    assert payload[1] == 6  # 3 * uint16
    assert payload[8] == TLV_NODE_STATUS
    assert payload[9] == 3
    assert payload[10] == 0b00000110
    assert payload[11] == 5
    assert payload[12] == 0

    crc_in = frame[-2] | (frame[-1] << 8)
    crc_calc = crc16_ccitt_false(frame[:-2])
    assert crc_in == crc_calc


def test_mesh_forward_rejects_ttl_zero_and_accepts_after_recrc() -> None:
    frame = bytearray(build_ping_frame(net_id=1, src_id=7, dst_id=0xFF, boot_id=1, seq=42))
    frame[7] = 0  # TTL
    crc = crc16_ccitt_false(bytes(frame[:-2]))
    frame[-2] = crc & 0xFF
    frame[-1] = (crc >> 8) & 0xFF

    assert frame_crc_ok(bytes(frame)) is True
    assert mesh_should_forward(bytes(frame), dedup_seen=False, rate_allow=True) is False

    frame[7] = 2
    crc = crc16_ccitt_false(bytes(frame[:-2]))
    frame[-2] = crc & 0xFF
    frame[-1] = (crc >> 8) & 0xFF
    assert mesh_should_forward(bytes(frame), dedup_seen=False, rate_allow=True) is True

    mutated = frame_dec_ttl_inc_hops_recrc(bytes(frame))
    assert mutated is not None
    assert mutated[7] == 1
    assert mutated[8] == 1
    assert frame_crc_ok(mutated) is True


def test_forward_queue_drop_newest_policy() -> None:
    q = ForwardQueue(capacity=4)
    assert q.push() is True
    assert q.push() is True
    assert q.push() is True
    assert q.push() is True
    assert q.size == 4
    assert q.push() is False  # newest dropped
    assert q.size == 4
    assert q.saturated_events == 1


def test_forward_window_limiter_resets_on_new_window() -> None:
    limiter = ForwardWindowLimiter(window_ms=10000, max_forwards_per_window=2)

    assert limiter.allow(1000) is True
    limiter.consume()
    assert limiter.allow(2000) is True
    limiter.consume()
    assert limiter.allow(3000) is False

    # New window opens, allow again.
    assert limiter.allow(12050) is True


def test_mesh_forward_rejects_no_relay_flag() -> None:
    frame = bytearray(build_ping_frame(net_id=1, src_id=9, dst_id=0xFF, boot_id=1, seq=77))
    frame[9] |= FRAME_FLAG_NO_RELAY
    crc = crc16_ccitt_false(bytes(frame[:-2]))
    frame[-2] = crc & 0xFF
    frame[-1] = (crc >> 8) & 0xFF

    assert frame_crc_ok(bytes(frame)) is True
    assert mesh_should_forward(bytes(frame), dedup_seen=False, rate_allow=True) is False


def test_mesh_forward_rejects_when_dedup_already_seen() -> None:
    frame = build_ping_frame(net_id=1, src_id=10, dst_id=0xFF, boot_id=2, seq=78)
    assert frame_crc_ok(frame) is True
    assert mesh_should_forward(frame, dedup_seen=True, rate_allow=True) is False
