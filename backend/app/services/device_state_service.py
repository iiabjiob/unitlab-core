import time
from typing import Optional

from app.infrastructure.redis.manager import RedisManager
from app.schemas.ws.events import DeviceStateEvent
from app.infrastructure.protocol.modes import State
from app.core.utils import to_str, to_int


class DeviceStateService:
    @staticmethod
    async def update_state(unit_id: str, hdr, decoded) -> tuple[bool, DeviceStateEvent | None]:
        """
        Update Redis state for the device based on the packet header/mode and decoded payload.
        Returns:
            changed: bool       -> True if Redis was updated
            event: DeviceStateEvent | None
        """
        redis = RedisManager.get_instance()
        changed = False

        if hdr.mode == State.STATE_ALL_BIT:
            current = await redis.get(f"device:{unit_id}:bitmask")
            new_val = str(decoded.bitmask)
            if to_str(current) != new_val:
                await redis.set(f"device:{unit_id}:bitmask", new_val)
                changed = True

        elif hdr.mode == State.STATE_SINGLE_BIT:
            current = await redis.get(f"device:{unit_id}:bitmask")
            mask_val = to_int(current, 0) or 0
            new_mask_val = mask_val
            if decoded.value:
                new_mask_val |= (1 << decoded.ch)
            else:
                new_mask_val &= ~(1 << decoded.ch)

            if new_mask_val != mask_val:
                await redis.set(f"device:{unit_id}:bitmask", str(new_mask_val))
                changed = True

        elif hdr.mode == State.STATE_SINGLE_FLOAT:
            current_val = await redis.hget(f"device:{unit_id}:ao", str(decoded.ch))
            new_val = str(decoded.value)
            if to_str(current_val) != new_val:
                await redis.hset(f"device:{unit_id}:ao", str(decoded.ch), new_val)
                changed = True

        event = DeviceStateEvent(
            unit_id=unit_id,
            timestamp=hdr.timestamp_ms or int(time.time() * 1000),
            mode=State(hdr.mode),
            payload=decoded.model_dump(),
        )
        return changed, event

    # -------------------------------------------------------------------------

    @staticmethod
    async def build_event_from_redis(unit_id: str, mode: State, ch: Optional[int] = None) -> DeviceStateEvent:
        """
        Build a DeviceStateEvent from current values stored in Redis (no writes).
        Useful for snapshots or forced resend.
        """
        redis = RedisManager.get_instance()

        if mode == State.STATE_ALL_BIT:
            bitmask = to_int(await redis.get(f"device:{unit_id}:bitmask"), 0)
            payload = {"bitmask": bitmask}

        elif mode == State.STATE_SINGLE_BIT:
            bitmask = to_int(await redis.get(f"device:{unit_id}:bitmask"), 0)
            value = 1 if (bitmask & (1 << ch)) else 0
            payload = {"ch": ch, "value": value}

        elif mode == State.STATE_SINGLE_FLOAT:
            val = await redis.hget(f"device:{unit_id}:ao", str(ch))
            payload = {"ch": ch, "value": float(to_str(val) or 0)}

        else:
            payload = {}

        return DeviceStateEvent(
            unit_id=unit_id,
            timestamp=int(time.time() * 1000),
            mode=mode,
            payload=payload,
        )

    # -------------------------------------------------------------------------

    @staticmethod
    async def get_snapshot(unit_id: str) -> list[DeviceStateEvent]:
        """
        Get a full snapshot of device state for UI sync:
        - Current bitmask
        - All AO channels
        """
        redis = RedisManager.get_instance()
        events: list[DeviceStateEvent] = []

        # Always include full bitmask
        events.append(await DeviceStateService.build_event_from_redis(unit_id, State.STATE_ALL_BIT))

        # Include AO channels if exist
        ao_channels = await redis.hkeys(f"device:{unit_id}:ao")
        for ch in ao_channels:
            events.append(
                await DeviceStateService.build_event_from_redis(unit_id, State.STATE_SINGLE_FLOAT, int(ch))
            )

        return events
