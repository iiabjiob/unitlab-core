# protocol/packet.py
# Mirror of C++ PacketBuilder / PacketParser

from typing import Optional
from .header import (
    HEADER_SIZE,
    MAX_PAYLOAD,
    PacketHeader,
    pack_header,
    unpack_header,
)


class PacketBuilder:
    """Helper to build packets (header + payload)."""

    def __init__(self, capacity: int = HEADER_SIZE + MAX_PAYLOAD):
        self.buf = bytearray(capacity)
        self.cap = capacity
        self.len = 0

    def build(self, mode: int, packet_id: int, ts: int, payload: bytes) -> bool:
        """Build full packet with header + payload into self.buf."""
        payload_len = len(payload)
        if payload_len > MAX_PAYLOAD:
            return False
        if self.cap < HEADER_SIZE + payload_len:
            return False

        # Create header
        hdr = PacketHeader(
            mode=mode,
            version=1,  # PROTOCOL_VERSION
            packet_id=packet_id,
            timestamp_ms=ts,
            payload_len=payload_len,
        )

        # Write header
        self.buf[0:HEADER_SIZE] = pack_header(hdr)
        # Write payload
        self.buf[HEADER_SIZE : HEADER_SIZE + payload_len] = payload

        self.len = HEADER_SIZE + payload_len
        return True

    def payload_view(self) -> memoryview:
        return memoryview(self.buf)[HEADER_SIZE:self.len]

    def total_size(self) -> int:
        return self.len

    def to_bytes(self) -> bytes:
        return bytes(self.buf[:self.len])


class PacketParser:
    """Helper to parse a raw packet (header + payload)."""

    def __init__(self, data: bytes):
        self.buf = data
        self.len = len(data)
        self.hdr: Optional[PacketHeader] = None

    def parse_header(self) -> bool:
        """Parse header, validate size, store in self.hdr."""
        if self.len < HEADER_SIZE:
            return False
        hdr = unpack_header(self.buf[0:HEADER_SIZE])
        if hdr.payload_len > MAX_PAYLOAD:
            return False
        if self.len < HEADER_SIZE + hdr.payload_len:
            return False
        self.hdr = hdr
        return True

    def payload(self) -> bytes:
        if self.hdr is None:
            raise ValueError("Header not parsed yet")
        return self.buf[HEADER_SIZE : HEADER_SIZE + self.hdr.payload_len]

    def payload_len(self) -> int:
        if self.hdr is None:
            raise ValueError("Header not parsed yet")
        return self.hdr.payload_len
