from __future__ import annotations

import json
from datetime import datetime, timezone
from collections.abc import Mapping
from typing import Protocol, cast
from uuid import uuid4

from pydantic import BaseModel, Field

from app.core.config import get_settings
from app.infrastructure.redis.manager import RedisManager

settings = get_settings()


class _CoreNtpRedis(Protocol):
    async def get(self, name: str) -> object: ...

    async def xadd(self, name: str, fields: Mapping[str, object], **kwargs: object) -> object: ...


class CoreNtpCommandAccepted(BaseModel):
    request_id: str
    action: str
    queued_at: datetime


class CoreNtpApplyServersPayload(BaseModel):
    servers: list[str] = Field(default_factory=list)


class CoreNtpSetTimePayload(BaseModel):
    timestamp: datetime


def _now_utc() -> datetime:
    return datetime.now(timezone.utc)


def _normalize_json_dict(raw: object) -> dict[str, object] | None:
    if raw is None:
        return None
    if isinstance(raw, dict):
        return cast(dict[str, object], raw)
    if isinstance(raw, str):
        try:
            payload = cast(object, json.loads(raw))
        except json.JSONDecodeError:
            return None
        return cast(dict[str, object], payload) if isinstance(payload, dict) else None
    return None


async def get_core_ntp_state() -> dict[str, object] | None:
    redis = cast(_CoreNtpRedis, cast(object, RedisManager.get_instance()))
    raw = await redis.get(settings.core_ntp_state_key)
    return _normalize_json_dict(raw)


async def enqueue_core_ntp_command(
    action: str,
    *,
    payload: dict[str, object] | None = None,
    request_id: str | None = None,
) -> CoreNtpCommandAccepted:
    redis = cast(_CoreNtpRedis, cast(object, RedisManager.get_instance()))
    queued_at = _now_utc()
    rid = request_id or uuid4().hex
    body: dict[str, object] = {"request_id": rid, "action": action, **(payload or {})}
    _ = await redis.xadd(
        settings.core_ntp_command_stream,
        {"json": json.dumps(body, ensure_ascii=True)},
        maxlen=settings.core_ntp_command_stream_maxlen,
        approximate=True,
    )
    return CoreNtpCommandAccepted(request_id=rid, action=action, queued_at=queued_at)
