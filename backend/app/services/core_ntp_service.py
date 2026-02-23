from __future__ import annotations

import json
from datetime import datetime, timezone
from typing import Any
from uuid import uuid4

from pydantic import BaseModel, Field

from app.core.config import get_settings
from app.infrastructure.redis.manager import RedisManager

settings = get_settings()


class CoreNtpCommandAccepted(BaseModel):
    request_id: str
    action: str
    queued_at: datetime


class CoreNtpApplyServersPayload(BaseModel):
    servers: list[str] = Field(default_factory=list)


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


async def get_core_ntp_state() -> dict[str, Any] | None:
    redis = RedisManager.get_instance()
    raw = await redis.get(settings.core_ntp_state_key)
    return _normalize_json_dict(raw)


async def enqueue_core_ntp_command(
    action: str,
    *,
    payload: dict[str, Any] | None = None,
    request_id: str | None = None,
) -> CoreNtpCommandAccepted:
    redis = RedisManager.get_instance()
    queued_at = _now_utc()
    rid = request_id or uuid4().hex
    body: dict[str, Any] = {"request_id": rid, "action": action, **(payload or {})}
    await redis.xadd(
        settings.core_ntp_command_stream,
        {"json": json.dumps(body, ensure_ascii=True)},
        maxlen=settings.core_ntp_command_stream_maxlen,
        approximate=True,
    )
    return CoreNtpCommandAccepted(request_id=rid, action=action, queued_at=queued_at)

