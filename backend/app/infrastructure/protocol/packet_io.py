
import time
from .header import PacketHeader, pack_header, PROTOCOL_VERSION

class PacketBuilder:
    """Builds a full packet (header + payload)."""
    def __init__(self, mode: int, packet_id: int | None = None, timestamp_ms: int | None = None):
        self.mode = mode
        self.packet_id = int(packet_id if packet_id is not None else 0)
        self.timestamp_ms = int(timestamp_ms if timestamp_ms is not None else int(time.time() * 1000))
        self.payload_chunks: list[bytes] = []

    def add(self, payload: bytes) -> None:
        self.payload_chunks.append(payload or b"")

    def build(self) -> bytes:
        payload = b"".join(self.payload_chunks)
        hdr = PacketHeader(
            mode=self.mode,
            version=PROTOCOL_VERSION,
            packet_id=self.packet_id & 0xFFFF,
            timestamp_ms=self.timestamp_ms & ((1<<64)-1),
            payload_len=len(payload) & 0xFFFF,
        )
        return pack_header(hdr) + payload
