import json
import time
from typing import Any

from app.infrastructure.mqtt.handler_registry import registry
from app.infrastructure.redis.manager import RedisManager
from app.infrastructure.mqtt import topics
from app.core.events.ws_event_publisher import WsEventPublisher
from app.schemas.ws.events import DeviceHeartbeatEvent
from app.services.command_queue_service import enqueue_scan_devices
from app.core.utils import to_str
from app.core.config import get_settings
from app.core.logger import get_logger

settings = get_settings()
logger = get_logger("mqtt")


def _heartbeat_kind_from_topic(topic: str) -> str | None:
    if topic.endswith('/hd'):
        return 'diag'
    if topic.endswith('/h'):
        return 'fast'
    return None


def _heartbeat_redis_key(unit_id: str, kind: str | None) -> str | None:
    if kind == 'fast':
        return f"device:{unit_id}:hb_fast"
    if kind == 'diag':
        return f"device:{unit_id}:hb_diag"
    return None


def _parse_heartbeat_payload(payload: bytes) -> dict[str, Any] | None:
    if not payload:
        return None
    try:
        decoded = json.loads(payload.decode('utf-8'))
    except Exception as exc:
        logger.warning("Invalid heartbeat JSON payload: %s", exc)
        return None
    if not isinstance(decoded, dict):
        logger.warning("Unexpected heartbeat payload type: %s", type(decoded).__name__)
        return None
    return decoded


@registry.mqtt_handler(topics.DEVICE_HEARTBEAT_DIAG)
@registry.mqtt_handler(topics.DEVICE_HEARTBEAT)
async def handle_device_heartbeat(topic: str, payload: bytes, unit_id: str):
    start_total = time.perf_counter()
    ts = int(time.time() * 1000)
    hb_kind = _heartbeat_kind_from_topic(topic)
    logger.debug(f"📥 IN ← {unit_id}: heartbeat ({hb_kind or 'unknown'}) @ {ts}")

    redis = RedisManager.get_instance()

    heartbeat_payload = _parse_heartbeat_payload(payload)
    heartbeat_fast = heartbeat_payload if hb_kind == 'fast' else None
    heartbeat_diag = heartbeat_payload if hb_kind == 'diag' else None

    telemetry_set_latency = 0.0
    telemetry_key = _heartbeat_redis_key(unit_id, hb_kind)
    if telemetry_key and heartbeat_payload is not None:
        start_telemetry_set = time.perf_counter()
        await redis.set(telemetry_key, json.dumps(heartbeat_payload, separators=(",", ":")))
        telemetry_set_latency = (time.perf_counter() - start_telemetry_set) * 1000

    # Update last_seen (this key uses TTL)
    start_redis_touch = time.perf_counter()
    await redis.set(
        f"device:{unit_id}:last_seen",
        str(ts).encode(),
        ex=settings.heartbeat_ttl
    )
    redis_touch_latency = (time.perf_counter() - start_redis_touch) * 1000

    # Add the device into the global set if it is new
    start_redis_sadd = time.perf_counter()
    await redis.sadd("devices:all", unit_id.encode())
    redis_sadd_latency = (time.perf_counter() - start_redis_sadd) * 1000

    # Check cached status
    start_status_get = time.perf_counter()
    prev_status_raw = await redis.get(f"device:{unit_id}:status")
    prev_status = to_str(prev_status_raw)
    status_get_latency = (time.perf_counter() - start_status_get) * 1000

    status_set_latency = 0.0
    ws_publish_latency = 0.0
    scan_enqueue_latency = 0.0
    transitioned_online = False

    if prev_status != "online":
        transitioned_online = True
        start_status_set = time.perf_counter()
        await redis.set(f"device:{unit_id}:status", "online")
        status_set_latency = (time.perf_counter() - start_status_set) * 1000

    event = DeviceHeartbeatEvent(
        unit_id=unit_id,
        status="online",
        last_seen=ts,
        heartbeat_kind=hb_kind,
        heartbeat_fast=heartbeat_fast,
        heartbeat_diag=heartbeat_diag,
    )
    start_ws_publish = time.perf_counter()
    await WsEventPublisher.publish(event)
    ws_publish_latency = (time.perf_counter() - start_ws_publish) * 1000

    if transitioned_online:
        logger.info(f"Device {unit_id} came online")

        # Request fresh device info and states
        start_enqueue_scan = time.perf_counter()
        await enqueue_scan_devices(correlation_id=0, unit_id=unit_id)
        scan_enqueue_latency = (time.perf_counter() - start_enqueue_scan) * 1000

    total_latency = (time.perf_counter() - start_total) * 1000
    logger.debug(
        f"[HBRT] {unit_id} | "
        f"kind={hb_kind or '-'}, "
        f"touch={redis_touch_latency:.1f} ms, "
        f"telemetry_set={telemetry_set_latency:.1f} ms, "
        f"sadd={redis_sadd_latency:.1f} ms, "
        f"status_get={status_get_latency:.1f} ms, "
        f"transition={'yes' if transitioned_online else 'no'}, "
        f"status_set={status_set_latency:.1f} ms, "
        f"ws={ws_publish_latency:.1f} ms, "
        f"scan={scan_enqueue_latency:.1f} ms, "
        f"total={total_latency:.1f} ms"
    )
