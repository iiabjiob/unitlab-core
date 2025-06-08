# app/core/protocol.py
import struct

def calc_crc16(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return crc

def parse_state_payload(payload: str) -> dict:
    """
    Parses and validates the state payload sent from ESP (hex string).
    Формат: [timestamp:4][channels:1][bitmask:4][crc16:2] = 11 байт (22 hex-символа)
    """
    if len(payload) != 22:
        raise ValueError(f"Payload has unexpected length: {len(payload)}")

    data = bytes.fromhex(payload[:18])  # первые 9 байт
    crc_recv = int(payload[18:], 16)
    crc_calc = calc_crc16(data)
    if crc_calc != crc_recv:
        raise ValueError(f"CRC mismatch: got {crc_recv:04X}, expected {crc_calc:04X}")

    # >IBI: big-endian, uint32, uint8, uint32
    timestamp, channels, bitmask = struct.unpack(">IBI", data)
    return {
        "timestamp": timestamp,
        "channels": channels,
        "bitmask": bitmask,
        "states": [(bitmask >> i) & 1 for i in range(channels)],
        "crc_ok": True,
    }

def build_do_command_packet(
    timestamp: int,
    channels: int,
    mode: int,
    delay_before_ms: int,
    pulse_ms: int,
    repeat: int,
    bitmask: int,
) -> bytes:
    """
    Формирует бинарный пакет команды управления DO:
    [timestamp:4][channels:1][mode:1][delay:2][pulse:2][repeat:1][bitmask:4][crc16:2]
    """
    data = struct.pack(">IBBHHBI",
        timestamp, channels, mode, delay_before_ms, pulse_ms, repeat, bitmask
    )
    crc = calc_crc16(data)
    return data + struct.pack(">H", crc)