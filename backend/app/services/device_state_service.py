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

        elif hdr.mode == State.DIAG_ALL_BIT:
            diag_key = f"device:{unit_id}:diag"
            current_diag = await redis.hgetall(diag_key)
            new_mapping = {
                "open_mask": str(decoded.open_mask),
                "fault_mask": str(decoded.fault_mask),
                "soft_mask": str(decoded.soft_mask),
            }
            if any(current_diag.get(k) != v for k, v in new_mapping.items()):
                await redis.hset(diag_key, mapping=new_mapping)
                changed = True

        elif hdr.mode == State.STATE_CHANGED_BIT:
            bitmask_key = f"device:{unit_id}:bitmask"
            current = await redis.get(bitmask_key)
            current_mask = to_int(current, 0) or 0
            changed_mask = int(decoded.changed) & 0xFFFFFFFF
            state_mask = int(decoded.state) & 0xFFFFFFFF
            next_mask = (current_mask & ~changed_mask) | (state_mask & changed_mask)
            if next_mask != current_mask:
                await redis.set(bitmask_key, str(next_mask))
                changed = True

        elif hdr.mode == State.DIAG_DI_BIT:
            diag_key = f"device:{unit_id}:diag_di"
            current_diag = await redis.hgetall(diag_key)
            new_mapping = {
                "seen": str(decoded.seen),
                "stuck": str(decoded.stuck),
                "lost": str(decoded.lost),
                "latched": str(decoded.latched),
                "latched_changed": str(decoded.latched_changed),
                "latched_cause": str(decoded.latched_cause),
            }
            if any(current_diag.get(k) != v for k, v in new_mapping.items()):
                await redis.hset(diag_key, mapping=new_mapping)
                changed = True

        elif hdr.mode == State.STATE_LATCHED_BIT:
            latched_key = f"device:{unit_id}:diag_di_latched"
            current_diag = await redis.hgetall(latched_key)
            new_mapping = {
                "latched": str(decoded.latched),
                "changed": str(decoded.changed),
                "cause": str(decoded.cause),
            }
            if any(current_diag.get(k) != v for k, v in new_mapping.items()):
                await redis.hset(latched_key, mapping=new_mapping)
                changed = True

        if not changed:
            return False, None

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

        elif mode == State.DIAG_ALL_BIT:
            diag = await redis.hgetall(f"device:{unit_id}:diag")
            payload = {
                "open_mask": to_int(diag.get("open_mask"), 0),
                "fault_mask": to_int(diag.get("fault_mask"), 0),
                "soft_mask": to_int(diag.get("soft_mask"), 0),
            }

        elif mode == State.DIAG_DI_BIT:
            diag = await redis.hgetall(f"device:{unit_id}:diag_di")
            payload = {
                "seen": to_int(diag.get("seen"), 0),
                "stuck": to_int(diag.get("stuck"), 0),
                "lost": to_int(diag.get("lost"), 0),
                "latched": to_int(diag.get("latched"), 0),
                "latched_changed": to_int(diag.get("latched_changed"), 0),
                "latched_cause": to_int(diag.get("latched_cause"), 0),
            }

        elif mode == State.STATE_LATCHED_BIT:
            diag = await redis.hgetall(f"device:{unit_id}:diag_di_latched")
            payload = {
                "latched": to_int(diag.get("latched"), 0),
                "changed": to_int(diag.get("changed"), 0),
                "cause": to_int(diag.get("cause"), 0),
            }

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
        - Aggregated DO diagnostics (if available)
        - Aggregated DI diagnostics + latched info (if available)
        - All AO channels
        """
        redis = RedisManager.get_instance()
        events: list[DeviceStateEvent] = []

        # Always include full bitmask
        events.append(await DeviceStateService.build_event_from_redis(unit_id, State.STATE_ALL_BIT))

        # Include DO diagnostics if stored
        diag_exists = await redis.hlen(f"device:{unit_id}:diag")
        if diag_exists:
            events.append(await DeviceStateService.build_event_from_redis(unit_id, State.DIAG_ALL_BIT))

        # Include DI diagnostics if stored
        di_diag_exists = await redis.hlen(f"device:{unit_id}:diag_di")
        if di_diag_exists:
            events.append(await DeviceStateService.build_event_from_redis(unit_id, State.DIAG_DI_BIT))

        di_latched_exists = await redis.hlen(f"device:{unit_id}:diag_di_latched")
        if di_latched_exists:
            events.append(await DeviceStateService.build_event_from_redis(unit_id, State.STATE_LATCHED_BIT))

        # Include AO channels if exist
        ao_channels = await redis.hkeys(f"device:{unit_id}:ao")
        for ch in ao_channels:
            events.append(
                await DeviceStateService.build_event_from_redis(unit_id, State.STATE_SINGLE_FLOAT, int(ch))
            )

        return events
