
# Protocol header layout mirrors C++ Header.h
# Layout (big-endian for integer fields):
# [mode:1][version:1][packet_id:2][timestamp_ms_utc:8][payload_len:2]
from dataclasses import dataclass
from .endian import write_u16_be, write_u64_be, read_u16_be, read_u64_be

PROTOCOL_VERSION: int = 1
HEADER_SIZE: int = 1 + 1 + 2 + 8 + 2  # 14 bytes

@dataclass
class PacketHeader:
    mode: int
    version: int
    packet_id: int
    timestamp_ms: int
    payload_len: int

def pack_header(h: PacketHeader) -> bytes:
    if not (0 <= h.mode <= 0xFF):
        raise ValueError("mode must be 0..255")
    if not (0 <= h.version <= 0xFF):
        raise ValueError("version must be 0..255")
    if not (0 <= h.packet_id <= 0xFFFF):
        raise ValueError("packet_id must be 0..65535")
    if not (0 <= h.payload_len <= 0xFFFF):
        raise ValueError("payload_len must be 0..65535")
    return bytes([h.mode, h.version]) + write_u16_be(h.packet_id) + write_u64_be(h.timestamp_ms) + write_u16_be(h.payload_len)

def unpack_header(buf: bytes) -> PacketHeader:
    if len(buf) < HEADER_SIZE:
        raise ValueError("buffer too small for header")
    mode = buf[0]
    version = buf[1]
    packet_id = read_u16_be(buf[2:4])
    timestamp_ms = read_u64_be(buf[4:12])
    payload_len = read_u16_be(buf[12:14])
    return PacketHeader(mode, version, packet_id, timestamp_ms, payload_len)
