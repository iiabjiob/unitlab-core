
# protocol/header.py
# Packet header mirrors C++ protocol/Header.h
# Layout (big-endian):
# [mode:1][version:1][packet_id:2][timestamp_ms:8][payload_len:2]

from dataclasses import dataclass
from . import endian

PROTOCOL_VERSION = 1
MAX_PAYLOAD = 1024

HEADER_SIZE = 14  # 1+1+2+8+2

OFFSET_MODE = 0
OFFSET_VERSION = 1
OFFSET_PACKET_ID = 2
OFFSET_TIMESTAMP = 4
OFFSET_LEN = 12
OFFSET_PAYLOAD = 14

@dataclass
class PacketHeader:
    mode: int
    version: int
    packet_id: int
    timestamp_ms: int
    payload_len: int

def pack_header(h: PacketHeader) -> bytes:
    """Assemble header into preallocated buffer, like C++ struct layout."""
    buf = bytearray(HEADER_SIZE)
    buf[OFFSET_MODE] = h.mode
    buf[OFFSET_VERSION] = h.version
    endian.write_u16_be(h.packet_id, into=buf, offset=OFFSET_PACKET_ID)
    endian.write_u64_be(h.timestamp_ms, into=buf, offset=OFFSET_TIMESTAMP)
    endian.write_u16_be(h.payload_len, into=buf, offset=OFFSET_LEN)
    return bytes(buf)

def unpack_header(buf: bytes) -> PacketHeader:
    if len(buf) < HEADER_SIZE:
        raise ValueError("buffer too small")
    mode = buf[OFFSET_MODE]
    version = buf[OFFSET_VERSION]
    packet_id = endian.read_u16_be(buf, OFFSET_PACKET_ID)
    timestamp_ms = endian.read_u64_be(buf, OFFSET_TIMESTAMP)
    payload_len = endian.read_u16_be(buf, OFFSET_LEN)
    if payload_len > MAX_PAYLOAD:
        raise ValueError(f"payload_len exceeds MAX_PAYLOAD ({MAX_PAYLOAD})")
    return PacketHeader(mode, version, packet_id, timestamp_ms, payload_len)
