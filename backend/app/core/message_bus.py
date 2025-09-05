# app/core/message_bus.py
import asyncio, time
from dataclasses import dataclass
from typing import Optional

# ---- DTOs for queues ----

@dataclass
class InboundMqttMsg:
    topic: str
    payload: bytes
    qos: int = 0
    # Optional millisecond timestamp when we received the packet
    ts_ms: Optional[int] = None

@dataclass
class OutboundCmdMsg:
    """Pre-built command ready to publish to MQTT."""
    topic: str
    payload: bytes
    qos: int = 0
    retain: bool = False
    # Optional correlation (e.g. WS request id) to match RESP later
    correlation_id: Optional[str] = None
    # Optional packet_id if you use it in header builder to correlate RESP
    packet_id: Optional[int] = None
    enqueued_at_ms: int = None

    def __post_init__(self):
        if self.enqueued_at_ms is None:
            self.enqueued_at_ms = int(time.time() * 1000)

class MessageBus:
    """Singleton-like bus with two bounded queues and worker task handles."""
    _instance: "MessageBus|None" = None

    def __init__(self, inbound_maxsize: int = 2000, outbound_maxsize: int = 2000):
        # Using bounded queues to apply backpressure
        self.inbound_mqtt_q: asyncio.Queue[InboundMqttMsg] = asyncio.Queue(maxsize=inbound_maxsize)
        self.outbound_cmd_q: asyncio.Queue[OutboundCmdMsg] = asyncio.Queue(maxsize=outbound_maxsize)
        self._tasks: list[asyncio.Task] = []

    @classmethod
    def get_instance(cls) -> "MessageBus":
        if cls._instance is None:
            cls._instance = MessageBus()
        return cls._instance

    def register_task(self, t: asyncio.Task) -> None:
        self._tasks.append(t)

    async def shutdown(self):
        # Graceful: cancel workers and drain queues if needed
        for t in self._tasks:
            t.cancel()
        await asyncio.gather(*self._tasks, return_exceptions=True)
