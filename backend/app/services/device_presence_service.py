from __future__ import annotations

from dataclasses import dataclass
from datetime import datetime, timezone
from typing import Iterable

from app.core.config import get_settings
from app.core.logger import get_logger
from app.core.utils import to_int, to_str
from app.infrastructure.redis.manager import RedisManager

settings = get_settings()
logger = get_logger("device-presence")


@dataclass(frozen=True)
class DevicePresence:
    unit_id: str
    status: str
    last_seen_ms: int | None

    @property
    def online(self) -> bool:
        if self.status in {"online", "offline"}:
            return self.status == "online"
        if self.last_seen_ms is None:
            return False
        ttl_ms = max(int(settings.heartbeat_ttl or 30), 1) * 1000
        now_ms = int(datetime.now(timezone.utc).timestamp() * 1000)
        return (now_ms - self.last_seen_ms) <= ttl_ms

    @property
    def last_seen_at(self) -> datetime | None:
        if self.last_seen_ms is None:
            return None
        return datetime.fromtimestamp(self.last_seen_ms / 1000, tz=timezone.utc)


class DevicePresenceService:
    def __init__(self) -> None:
        self._redis = self._try_get_redis()

    def _try_get_redis(self):
        try:
            return RedisManager.get_instance()
        except RuntimeError:
            return None

    async def get_presence(self, unit_id: str) -> DevicePresence:
        result = await self.get_presence_map([unit_id])
        return result.get(unit_id, DevicePresence(unit_id=unit_id, status="offline", last_seen_ms=None))

    async def get_presence_map(self, unit_ids: Iterable[str]) -> dict[str, DevicePresence]:
        normalized_unit_ids: list[str] = []
        seen: set[str] = set()
        for raw_unit_id in unit_ids:
            unit_id = str(raw_unit_id).strip()
            if not unit_id or unit_id in seen:
                continue
            seen.add(unit_id)
            normalized_unit_ids.append(unit_id)
        if not normalized_unit_ids:
            return {}

        if self._redis is None:
            return {
                unit_id: DevicePresence(unit_id=unit_id, status="offline", last_seen_ms=None)
                for unit_id in normalized_unit_ids
            }

        pipe = self._redis.pipeline(transaction=False)
        for unit_id in normalized_unit_ids:
            pipe.get(f"device:{unit_id}:status")
            pipe.get(f"device:{unit_id}:last_seen")
        try:
            raw_values = await pipe.execute()
        except Exception as exc:  # pragma: no cover - defensive fallback for degraded Redis
            logger.warning(f"Presence Redis lookup failed, fallback to offline map: {exc}")
            return {
                unit_id: DevicePresence(unit_id=unit_id, status="offline", last_seen_ms=None)
                for unit_id in normalized_unit_ids
            }

        presence_by_unit_id: dict[str, DevicePresence] = {}
        ttl_ms = max(int(settings.heartbeat_ttl or 30), 1) * 1000
        now_ms = int(datetime.now(timezone.utc).timestamp() * 1000)

        for index, unit_id in enumerate(normalized_unit_ids):
            status_raw = raw_values[index * 2]
            last_seen_raw = raw_values[index * 2 + 1]
            last_seen_ms = to_int(last_seen_raw)
            status = to_str(status_raw, "")
            if status not in {"online", "offline"}:
                if last_seen_ms is None:
                    status = "offline"
                else:
                    status = "online" if (now_ms - last_seen_ms) <= ttl_ms else "offline"
            presence_by_unit_id[unit_id] = DevicePresence(
                unit_id=unit_id,
                status=status,
                last_seen_ms=last_seen_ms,
            )

        return presence_by_unit_id
