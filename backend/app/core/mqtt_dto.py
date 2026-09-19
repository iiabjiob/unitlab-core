from __future__ import annotations

import time
from dataclasses import dataclass, field


@dataclass(slots=True)
class InboundMqttMsg:
    """Inbound MQTT message already stripped from transport-level details."""

    topic: str
    payload: bytes
    qos: int = 0
    retain: bool = False
    ts_ms: int | None = None
    encoding: str = "binary"
    meta: dict[str, object] | None = None

    def __post_init__(self):
        if self.ts_ms is None:
            self.ts_ms = int(time.time() * 1000)


@dataclass(slots=True)
class OutboundCmdMsg:
    """Command prepared for publication via MQTT."""

    topic: str
    payload: bytes
    qos: int = 0
    retain: bool = False
    correlation_id: str | None = None
    command_id: str | None = None
    packet_id: int | None = None
    enqueued_at_ms: int = field(default_factory=lambda: int(time.time() * 1000))

    def to_metadata(self) -> dict[str, object]:
        return {
            "correlation_id": self.correlation_id,
            "command_id": self.command_id,
            "packet_id": self.packet_id,
        }
