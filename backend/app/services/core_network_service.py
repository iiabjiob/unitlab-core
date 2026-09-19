from __future__ import annotations

import json
from datetime import datetime, timezone
from collections.abc import Mapping
from typing import Literal, Protocol, cast
from uuid import uuid4

from pydantic import BaseModel, Field

from app.core.config import get_settings
from app.infrastructure.redis.manager import RedisManager

settings = get_settings()


class _CoreNetworkRedis(Protocol):
    async def get(self, name: str) -> object: ...

    async def xadd(self, name: str, fields: Mapping[str, object], **kwargs: object) -> object: ...


class CoreNetworkCommandAccepted(BaseModel):
    request_id: str
    action: str
    queued_at: datetime


class CoreNetworkConnectPayload(BaseModel):
    ssid: str
    password: str | None = None
    hidden: bool = False
    timeout_sec: int | None = Field(default=None, ge=5, le=120)


class CoreNetworkCommandScanPayload(BaseModel):
    timeout_sec: int | None = Field(default=None, ge=5, le=120)


class CoreNetworkProbeAddressesPayload(BaseModel):
    interface: str
    addresses: list[str] = Field(default_factory=list, min_length=1, max_length=512)
    timeout_sec: int | None = Field(default=None, ge=1, le=10)


class CoreNetworkApplySettingsPayload(BaseModel):
    interface: str | None = None
    profile: str | None = None
    ipv4_mode: Literal["auto", "manual"] = "auto"
    address_cidr: str | None = None
    gateway: str | None = None
    dns_servers: list[str] = Field(default_factory=list)
    proxy_url: str | None = None
    proxy_no_proxy: list[str] = Field(default_factory=list)


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


async def get_core_network_state() -> dict[str, object] | None:
    redis = cast(_CoreNetworkRedis, cast(object, RedisManager.get_instance()))
    raw = await redis.get(settings.core_net_state_key)
    return _normalize_json_dict(raw)


async def enqueue_core_network_command(
    action: str,
    *,
    payload: dict[str, object] | None = None,
    request_id: str | None = None,
) -> CoreNetworkCommandAccepted:
    redis = cast(_CoreNetworkRedis, cast(object, RedisManager.get_instance()))
    queued_at = _now_utc()
    rid = request_id or uuid4().hex
    body: dict[str, object] = {
        "request_id": rid,
        "action": action,
        **(payload or {}),
    }
    _ = await redis.xadd(
        settings.core_net_command_stream,
        {"json": json.dumps(body, ensure_ascii=True)},
        maxlen=settings.core_net_command_stream_maxlen,
        approximate=True,
    )
    return CoreNetworkCommandAccepted(request_id=rid, action=action, queued_at=queued_at)
