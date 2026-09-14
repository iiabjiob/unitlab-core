from __future__ import annotations

import json
from dataclasses import dataclass
from datetime import UTC, datetime, timedelta
from uuid import uuid4


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

    def __init__(self, redis, *, ttl_seconds: int = 30):
        self.redis = redis
        self.ttl_seconds = max(1, int(ttl_seconds))

    @staticmethod
    def _key(channel_id: int) -> str:
        return f"hardware:channel-lease:{int(channel_id)}"

    async def acquire(self, *, channel_id: int, owner_kind: str, owner_id: str) -> HardwareChannelLease | None:
        if int(channel_id) <= 0 or not owner_kind.strip() or not owner_id.strip():
            raise ValueError("channel_id, owner_kind and owner_id are required")
        lease_id = uuid4().hex
        epoch = int(await self.redis.incr(f"hardware:channel-epoch:{int(channel_id)}"))
        expires_at = datetime.now(UTC) + timedelta(seconds=self.ttl_seconds)
        payload = json.dumps({
            "lease_id": lease_id,
            "owner_kind": owner_kind.strip(),
            "owner_id": owner_id.strip(),
            "fencing_epoch": epoch,
        }, separators=(",", ":"))
        acquired = await self.redis.set(self._key(channel_id), payload, ex=self.ttl_seconds, nx=True)
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
        result = await self.redis.eval(script, 1, self._key(lease.channel_id), lease.lease_id)
        return bool(result)
