from __future__ import annotations

import json
from datetime import datetime, timezone
from typing import Any
from uuid import uuid4

from pydantic import BaseModel

from app.core.config import get_settings
from app.infrastructure.redis.manager import RedisManager

settings = get_settings()


class CoreDiagCommandAccepted(BaseModel):
    request_id: str
    action: str
    queued_at: datetime


def _now_utc() -> datetime:
    return datetime.now(timezone.utc)


def _normalize_json_dict(raw: Any) -> dict[str, Any] | None:
    if raw is None:
        return None
    if isinstance(raw, dict):
        return raw
    if isinstance(raw, str):
        try:
            payload = json.loads(raw)
        except json.JSONDecodeError:
            return None
        return payload if isinstance(payload, dict) else None
    return None


async def get_core_diag_state() -> dict[str, Any] | None:
    redis = RedisManager.get_instance()
    raw = await redis.get(settings.core_diag_state_key)
    return _normalize_json_dict(raw)


async def enqueue_core_diag_command(
    action: str,
    *,
    payload: dict[str, Any] | None = None,
    request_id: str | None = None,
) -> CoreDiagCommandAccepted:
    redis = RedisManager.get_instance()
    queued_at = _now_utc()
    rid = request_id or uuid4().hex
    body: dict[str, Any] = {"request_id": rid, "action": action, **(payload or {})}
    await redis.xadd(
        settings.core_diag_command_stream,
        {"json": json.dumps(body, ensure_ascii=True)},
        maxlen=settings.core_diag_command_stream_maxlen,
        approximate=True,
    )
    return CoreDiagCommandAccepted(request_id=rid, action=action, queued_at=queued_at)

