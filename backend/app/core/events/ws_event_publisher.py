from __future__ import annotations

from typing import Any, Dict

from pydantic import BaseModel

from app.core.logger import get_logger
from app.infrastructure.redis.stream_bus import publish_ws_event
from app.infrastructure.protocol.modes import State

logger = get_logger("ws.pub")

_SUPPRESS_WS_DIAGNOSTICS = False
_SUPPRESS_WS_DEVICE_STATUS = False


def _build_payload(event: BaseModel) -> Dict[str, Any]:
    data = event.model_dump(mode="json")
    return {
        "event": event.__class__.__name__,
        "channel": data.get("channel"),
        "payload": data,
    }


def _should_publish_event(event: BaseModel) -> bool:
    if not _SUPPRESS_WS_DIAGNOSTICS:
        if _SUPPRESS_WS_DEVICE_STATUS:
            return getattr(event, "channel", None) != "devices/status"
        return True

    channel = getattr(event, "channel", None)
    if channel == "devices/status":
        if _SUPPRESS_WS_DEVICE_STATUS:
            return False
        if getattr(event, "heartbeat_kind", None) == "diag":
            return False

    if channel == "devices/state":
        mode = getattr(event, "mode", None)
        return mode not in {
            State.DIAG_ALL_BIT,
            State.DIAG_DI_BIT,
            State.DIAG_AO_FLOAT,
            State.STATE_LATCHED_BIT,
        }

    return True


class WsEventPublisher:
    """Publishes WS events into Redis Pub/Sub."""

    @staticmethod
    async def publish(event: BaseModel) -> None:
        if not _should_publish_event(event):
            logger.debug("WS diagnostics suppressed: %s", event.__class__.__name__)
            return
        try:
            await publish_ws_event(_build_payload(event))
        except Exception as exc:
            logger.error("💥 Failed to publish WS event %s: %s", event.__class__.__name__, exc)
            raise
