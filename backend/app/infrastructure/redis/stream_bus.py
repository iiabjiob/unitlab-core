from __future__ import annotations

import base64
import json
from typing import Protocol, cast
from collections.abc import Awaitable, Callable, Mapping

from redis.asyncio.client import PubSub

from app.core.config import get_settings
from app.core.mqtt_dto import InboundMqttMsg, OutboundCmdMsg
from app.core.sequence_dto import SequenceCommand, SequenceEvent
from app.infrastructure.redis.manager import RedisManager

settings = get_settings()

StreamEntry = tuple[str, dict[str, object]]


class StreamRedisClient(Protocol):
    async def xadd(self, *args: object, **kwargs: object) -> str: ...

    async def publish(self, channel: str, message: str) -> int: ...

    def pubsub(self) -> PubSub: ...


def _redis_client() -> StreamRedisClient:
    return cast(StreamRedisClient, cast(object, RedisManager.get_instance()))


def _encode_bytes(data: bytes) -> str:
    return base64.b64encode(data).decode("ascii")


def _decode_bytes(data: str) -> bytes:
    return base64.b64decode(data.encode("ascii"))


def _wrap_payload(payload: Mapping[str, object]) -> dict[str, str]:
    return {"data": json.dumps(payload, separators=(",", ":"))}


def _unwrap_payload(entry: StreamEntry) -> tuple[str, dict[str, object]]:
    entry_id, fields = entry
    raw = fields.get("data")
    if raw is None:
        raise ValueError(f"Stream entry {entry_id} missing 'data' field")
    document = cast(object, json.loads(_as_str(raw)))
    if not isinstance(document, dict):
        raise ValueError(f"Stream entry {entry_id} data must be a JSON object")
    return entry_id, cast(dict[str, object], document)


def _as_str(value: object, default: str = "") -> str:
    return value if isinstance(value, str) else default


def _optional_str(value: object) -> str | None:
    return value if isinstance(value, str) else None


def _as_int(value: object, default: int = 0) -> int:
    try:
        return int(cast(str | int | float, value))
    except (TypeError, ValueError):
        return default


def _as_bool(value: object, default: bool = False) -> bool:
    return value if isinstance(value, bool) else default


def _as_object_dict(value: object) -> dict[str, object]:
    return cast(dict[str, object], value) if isinstance(value, dict) else {}


async def enqueue_inbound_message(msg: InboundMqttMsg) -> str:
    redis = _redis_client()
    payload = {
        "topic": msg.topic,
        "payload": _encode_bytes(msg.payload),
        "qos": msg.qos,
        "retain": msg.retain,
        "ts_ms": msg.ts_ms,
        "encoding": msg.encoding,
        "meta": msg.meta or {},
    }
    return await redis.xadd(
        settings.mqtt_in_stream,
        _wrap_payload(payload),
        maxlen=settings.mqtt_stream_maxlen,
        approximate=True,
    )


async def enqueue_outbound_command(msg: OutboundCmdMsg) -> str:
    redis = _redis_client()
    payload = {
        "topic": msg.topic,
        "payload": _encode_bytes(msg.payload),
        "qos": msg.qos,
        "retain": msg.retain,
        "enqueued_at_ms": msg.enqueued_at_ms,
        "correlation_id": msg.correlation_id,
        "command_id": msg.command_id,
        "packet_id": msg.packet_id,
    }
    return await redis.xadd(
        settings.mqtt_out_stream,
        _wrap_payload(payload),
        maxlen=settings.mqtt_stream_maxlen,
        approximate=True,
    )


def parse_inbound_entry(entry: StreamEntry) -> tuple[str, InboundMqttMsg]:
    entry_id, payload = _unwrap_payload(entry)
    msg = InboundMqttMsg(
        topic=_as_str(payload.get("topic")),
        payload=_decode_bytes(_as_str(payload.get("payload"))),
        qos=_as_int(payload.get("qos")),
        retain=_as_bool(payload.get("retain")),
        ts_ms=_as_int(payload["ts_ms"]) if payload.get("ts_ms") is not None else None,
        encoding=_as_str(payload.get("encoding"), "binary"),
        meta=_as_object_dict(payload.get("meta")),
    )
    return entry_id, msg


def parse_outbound_entry(entry: StreamEntry) -> tuple[str, OutboundCmdMsg]:
    entry_id, payload = _unwrap_payload(entry)
    msg = OutboundCmdMsg(
        topic=_as_str(payload.get("topic")),
        payload=_decode_bytes(_as_str(payload.get("payload"))),
        qos=_as_int(payload.get("qos")),
        retain=_as_bool(payload.get("retain")),
        correlation_id=_optional_str(payload.get("correlation_id")),
        command_id=_optional_str(payload.get("command_id")),
        packet_id=_as_int(payload["packet_id"]) if payload.get("packet_id") is not None else None,
        enqueued_at_ms=_as_int(payload.get("enqueued_at_ms") or payload.get("ts_ms")),
    )
    return entry_id, msg


async def enqueue_sequence_command(cmd: SequenceCommand, *, stream_name: str | None = None) -> str:
    redis = _redis_client()
    target_stream = stream_name or settings.sequence_command_stream
    return await redis.xadd(
        target_stream,
        _wrap_payload(cmd.to_payload()),
        maxlen=settings.sequence_stream_maxlen,
        approximate=True,
    )


def parse_sequence_command_entry(entry: StreamEntry) -> tuple[str, SequenceCommand]:
    entry_id, payload = _unwrap_payload(entry)
    return entry_id, SequenceCommand.from_payload(payload)


async def append_sequence_event(event: SequenceEvent) -> str:
    redis = _redis_client()
    return await redis.xadd(
        settings.sequence_event_stream,
        _wrap_payload(event.to_payload()),
        maxlen=settings.sequence_stream_maxlen,
        approximate=True,
    )


def parse_sequence_event_entry(entry: StreamEntry) -> tuple[str, SequenceEvent]:
    entry_id, payload = _unwrap_payload(entry)
    return entry_id, SequenceEvent.from_payload(payload)


async def enqueue_signal_allocation_job(payload: Mapping[str, object]) -> str:
    redis = _redis_client()
    return await redis.xadd(
        settings.signal_allocation_job_stream,
        _wrap_payload(payload),
        maxlen=settings.signal_allocation_job_stream_maxlen,
        approximate=True,
    )


async def enqueue_signal_test_run_job(payload: Mapping[str, object]) -> str:
    redis = _redis_client()
    return await redis.xadd(
        settings.signal_test_run_job_stream,
        _wrap_payload(payload),
        maxlen=settings.signal_test_run_job_stream_maxlen,
        approximate=True,
    )


def parse_signal_allocation_job_entry(entry: StreamEntry) -> tuple[str, dict[str, object]]:
    return _unwrap_payload(entry)


async def publish_ws_event(payload: dict[str, object]) -> None:
    redis = _redis_client()
    _ = await redis.publish(settings.ws_events_channel, json.dumps(payload, separators=(",", ":")))


async def subscribe_ws_events() -> PubSub:
    redis = _redis_client()
    pubsub = redis.pubsub()
    subscribe = cast(Callable[..., Awaitable[object]], getattr(pubsub, "subscribe"))
    _ = await subscribe(settings.ws_events_channel)
    return pubsub
