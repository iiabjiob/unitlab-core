from __future__ import annotations

import asyncio
import json
from contextlib import suppress

from app.core.logger import get_logger
from app.infrastructure.redis.stream_bus import subscribe_ws_events
from app.ws.manager import WebSocketManager

logger = get_logger("ws.sub")


async def forward_ws_events_from_pubsub():
    """Listen to the Redis Pub/Sub channel and forward events to WebSocketManager."""

    pubsub = await subscribe_ws_events()
    manager = WebSocketManager.get_instance()

    try:
        async for message in pubsub.listen():
            if message["type"] != "message":
                continue

            raw = message.get("data")
            if raw is None:
                continue

            try:
                payload = json.loads(raw)
            except json.JSONDecodeError as exc:
                logger.error("💥 Invalid WS payload: %s", exc)
                continue

            data = payload.get("payload", payload)
            await manager.broadcast(data)
    except asyncio.CancelledError:
        raise
    except Exception as exc:
        logger.error("💥 Error inside WS Pub/Sub listener: %s", exc)
        raise
    finally:
        with suppress(asyncio.CancelledError):
            await pubsub.unsubscribe()
            await pubsub.close()
