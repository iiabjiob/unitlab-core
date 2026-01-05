from __future__ import annotations

from typing import Any, Dict

from pydantic import BaseModel

from app.core.logger import get_logger
from app.infrastructure.redis.stream_bus import publish_ws_event

logger = get_logger("ws.pub")


def _build_payload(event: BaseModel) -> Dict[str, Any]:
    data = event.model_dump(mode="json")
    return {
        "event": event.__class__.__name__,
        "channel": data.get("channel"),
        "payload": data,
    }


class WsEventPublisher:
    """Publishes WS events into Redis Pub/Sub."""

    @staticmethod
    async def publish(event: BaseModel) -> None:
        try:
            await publish_ws_event(_build_payload(event))
        except Exception as exc:
            logger.error("💥 Failed to publish WS event %s: %s", event.__class__.__name__, exc)
            raise
