from __future__ import annotations

import base64
import json
from typing import Any, Dict, Tuple

from redis.asyncio.client import PubSub

from app.core.config import get_settings
from app.core.mqtt_dto import InboundMqttMsg, OutboundCmdMsg
from app.core.sequence_dto import SequenceCommand, SequenceEvent
from app.infrastructure.redis.manager import RedisManager

settings = get_settings()

StreamEntry = Tuple[str, Dict[str, str]]


def _encode_bytes(data: bytes) -> str:
    return base64.b64encode(data).decode("ascii")


def _decode_bytes(data: str) -> bytes:
    return base64.b64decode(data.encode("ascii"))


def _wrap_payload(payload: Dict[str, Any]) -> Dict[str, str]:
    return {"data": json.dumps(payload, separators=(",", ":"))}


def _unwrap_payload(entry: StreamEntry) -> Tuple[str, Dict[str, Any]]:
    entry_id, fields = entry
    raw = fields.get("data")
    if raw is None:
        raise ValueError(f"Stream entry {entry_id} missing 'data' field")
    return entry_id, json.loads(raw)


async def enqueue_inbound_message(msg: InboundMqttMsg) -> str:
    redis = RedisManager.get_instance()
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
    redis = RedisManager.get_instance()
    payload = {
        "topic": msg.topic,
        "payload": _encode_bytes(msg.payload),
        "qos": msg.qos,
        "retain": msg.retain,
        "enqueued_at_ms": msg.enqueued_at_ms,
        "correlation_id": msg.correlation_id,
        "packet_id": msg.packet_id,
    }
    return await redis.xadd(
        settings.mqtt_out_stream,
        _wrap_payload(payload),
        maxlen=settings.mqtt_stream_maxlen,
        approximate=True,
    )


def parse_inbound_entry(entry: StreamEntry) -> Tuple[str, InboundMqttMsg]:
    entry_id, payload = _unwrap_payload(entry)
    msg = InboundMqttMsg(
        topic=payload["topic"],
        payload=_decode_bytes(payload["payload"]),
        qos=payload.get("qos", 0),
        retain=payload.get("retain", False),
        ts_ms=payload.get("ts_ms"),
        encoding=payload.get("encoding", "binary"),
        meta=payload.get("meta") or {},
    )
    return entry_id, msg


def parse_outbound_entry(entry: StreamEntry) -> Tuple[str, OutboundCmdMsg]:
    entry_id, payload = _unwrap_payload(entry)
    msg = OutboundCmdMsg(
        topic=payload["topic"],
        payload=_decode_bytes(payload["payload"]),
        qos=payload.get("qos", 0),
        retain=payload.get("retain", False),
        correlation_id=payload.get("correlation_id"),
        packet_id=payload.get("packet_id"),
        enqueued_at_ms=payload.get("enqueued_at_ms") or payload.get("ts_ms"),
    )
    return entry_id, msg


async def enqueue_sequence_command(cmd: SequenceCommand, *, stream_name: str | None = None) -> str:
    redis = RedisManager.get_instance()
    target_stream = stream_name or settings.sequence_command_stream
    return await redis.xadd(
        target_stream,
        _wrap_payload(cmd.to_payload()),
        maxlen=settings.sequence_stream_maxlen,
        approximate=True,
    )


def parse_sequence_command_entry(entry: StreamEntry) -> Tuple[str, SequenceCommand]:
    entry_id, payload = _unwrap_payload(entry)
    return entry_id, SequenceCommand.from_payload(payload)


async def append_sequence_event(event: SequenceEvent) -> str:
    redis = RedisManager.get_instance()
    return await redis.xadd(
        settings.sequence_event_stream,
        _wrap_payload(event.to_payload()),
        maxlen=settings.sequence_stream_maxlen,
        approximate=True,
    )


def parse_sequence_event_entry(entry: StreamEntry) -> Tuple[str, SequenceEvent]:
    entry_id, payload = _unwrap_payload(entry)
    return entry_id, SequenceEvent.from_payload(payload)


async def enqueue_signal_allocation_job(payload: Dict[str, Any]) -> str:
    redis = RedisManager.get_instance()
    return await redis.xadd(
        settings.signal_allocation_job_stream,
        _wrap_payload(payload),
        maxlen=settings.signal_allocation_job_stream_maxlen,
        approximate=True,
    )


async def enqueue_signal_test_run_job(payload: Dict[str, Any]) -> str:
    redis = RedisManager.get_instance()
    return await redis.xadd(
        settings.signal_test_run_job_stream,
        _wrap_payload(payload),
        maxlen=settings.signal_test_run_job_stream_maxlen,
        approximate=True,
    )


def parse_signal_allocation_job_entry(entry: StreamEntry) -> Tuple[str, Dict[str, Any]]:
    return _unwrap_payload(entry)


async def publish_ws_event(payload: Dict[str, Any]) -> None:
    redis = RedisManager.get_instance()
    await redis.publish(settings.ws_events_channel, json.dumps(payload, separators=(",", ":")))


async def subscribe_ws_events() -> PubSub:
    redis = RedisManager.get_instance()
    pubsub = redis.pubsub()
    await pubsub.subscribe(settings.ws_events_channel)
    return pubsub
