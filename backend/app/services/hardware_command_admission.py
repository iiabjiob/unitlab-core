from __future__ import annotations

import json
from dataclasses import dataclass
from datetime import UTC, datetime, timedelta
from typing import cast
from uuid import uuid4
import redis.asyncio as redis


@dataclass(frozen=True, slots=True)
class HardwareChannelLease:
    channel_id: int
    owner_kind: str
    owner_id: str
    fencing_epoch: int
    lease_id: str
    expires_at: datetime


class HardwareCommandAdmission:
    """Cross-process exclusive lease for a physical output channel."""

    def __init__(self, redis_client: redis.Redis, *, ttl_seconds: int = 30):
        self.redis: redis.Redis = redis_client
        self.ttl_seconds: int = max(1, int(ttl_seconds))

    @staticmethod
    def _key(channel_id: int) -> str:
        return f"hardware:channel-lease:{int(channel_id)}"

    async def acquire(self, *, channel_id: int, owner_kind: str, owner_id: str) -> HardwareChannelLease | None:
        if int(channel_id) <= 0 or not owner_kind.strip() or not owner_id.strip():
            raise ValueError("channel_id, owner_kind and owner_id are required")
        lease_id = uuid4().hex
        epoch = int(cast(int | str | float, cast(object, await self.redis.incr(f"hardware:channel-epoch:{int(channel_id)}"))))
        expires_at = datetime.now(UTC) + timedelta(seconds=self.ttl_seconds)
        payload = json.dumps({
            "lease_id": lease_id,
            "owner_kind": owner_kind.strip(),
            "owner_id": owner_id.strip(),
            "fencing_epoch": epoch,
        }, separators=(",", ":"))
        acquired = cast(object, await self.redis.set(self._key(channel_id), payload, ex=self.ttl_seconds, nx=True))
        if not acquired:
            return None
        return HardwareChannelLease(
            channel_id=int(channel_id),
            owner_kind=owner_kind.strip(),
            owner_id=owner_id.strip(),
            fencing_epoch=epoch,
            lease_id=lease_id,
            expires_at=expires_at,
        )

    async def acquire_many(
        self,
        *,
        channel_ids: list[int],
        owner_kind: str,
        owner_id: str,
    ) -> list[HardwareChannelLease] | None:
        normalized = list(dict.fromkeys(int(channel_id) for channel_id in channel_ids))
        if not normalized or any(channel_id <= 0 for channel_id in normalized):
            raise ValueError("channel_ids must contain positive ids")
        if not owner_kind.strip() or not owner_id.strip():
            raise ValueError("owner_kind and owner_id are required")
        lease_id = uuid4().hex
        now = datetime.now(UTC)
        expires_at = now + timedelta(seconds=self.ttl_seconds)
        lease_keys = [self._key(channel_id) for channel_id in normalized]
        epoch_keys = [f"hardware:channel-epoch:{channel_id}" for channel_id in normalized]
        script = """
        local count = tonumber(ARGV[5])
        for i = 1, count do
          if redis.call('EXISTS', KEYS[i]) == 1 then return {} end
        end
        local epochs = {}
        for i = 1, count do
          local epoch = redis.call('INCR', KEYS[count + i])
          local payload = cjson.encode({lease_id=ARGV[3], owner_kind=ARGV[1], owner_id=ARGV[2], fencing_epoch=epoch})
          redis.call('SET', KEYS[i], payload, 'EX', ARGV[4])
          epochs[i] = epoch
        end
        return epochs
        """
        raw_epochs = cast(object, await self.redis.eval(  # pyright: ignore[reportGeneralTypeIssues]
            script,
            len(lease_keys) + len(epoch_keys),
            *(lease_keys + epoch_keys),
            owner_kind.strip(),
            owner_id.strip(),
            lease_id,
            self.ttl_seconds,
            len(normalized),
        ))
        if not isinstance(raw_epochs, list):
            return None
        typed_epochs = cast(list[object], raw_epochs)
        if len(typed_epochs) != len(normalized):
            return None
        return [
            HardwareChannelLease(
                channel_id=channel_id,
                owner_kind=owner_kind.strip(),
                owner_id=owner_id.strip(),
                fencing_epoch=int(cast(int | str | float, epoch)),
                lease_id=lease_id,
                expires_at=expires_at,
            )
            for channel_id, epoch in zip(normalized, typed_epochs)
        ]

    async def is_current(self, lease: HardwareChannelLease) -> bool:
        """Return whether this owner still holds the channel lease fence."""
        raw = cast(object, await self.redis.get(self._key(lease.channel_id)))
        if isinstance(raw, bytes):
            raw = raw.decode("utf-8", errors="replace")
        if not isinstance(raw, (str, bytes, bytearray)) or not raw:
            return False
        try:
            current_value = cast(object, json.loads(raw))
        except (TypeError, ValueError):
            return False
        if not isinstance(current_value, dict):
            return False
        current = cast(dict[str, object], current_value)
        return (
            str(current.get("lease_id") or "") == lease.lease_id
            and str(current.get("owner_kind") or "") == lease.owner_kind
            and str(current.get("owner_id") or "") == lease.owner_id
            and int(cast(int | str | float, current.get("fencing_epoch") or 0)) == lease.fencing_epoch
        )

    async def release(self, lease: HardwareChannelLease) -> bool:
        script = """
        local current = redis.call('GET', KEYS[1])
        if not current then return 0 end
        local expected = ARGV[1]
        if string.find(current, expected, 1, true) then
          return redis.call('DEL', KEYS[1])
        end
        return 0
        """
        result = cast(object, await self.redis.eval(  # pyright: ignore[reportGeneralTypeIssues]
            script, 1, self._key(lease.channel_id), lease.lease_id
        ))
        return bool(result)
