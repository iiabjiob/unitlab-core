import time
from typing import Protocol, cast

from app.infrastructure.redis.manager import RedisManager
from app.schemas.ws.events import DeviceStateEvent
from app.infrastructure.protocol.modes import State
from app.core.utils import to_str, to_int
from app.infrastructure.redis.types import RedisHashClient
from app.infrastructure.protocol.header import PacketHeader


class _DecodedState(Protocol):
    bitmask: int
    ch: int
    value: int | float
    valid_mask: int
    pending_mask: int
    fault_mask: int
    error_mask: int
    open_mask: int
    soft_mask: int
    changed: int
    state: int
    seen: int
    stuck: int
    lost: int
    latched: int
    latched_changed: int
    latched_cause: int
    cause: int

    def model_dump(self) -> dict[str, object]: ...


class DeviceStateService:
    @staticmethod
    async def _should_emit_unchanged_event(unit_id: str, raw_mode: int, redis: RedisHashClient) -> bool:
        """Allow authoritative state snapshots for controllable devices without flooding passive DI streams."""
        if raw_mode == State.STATE_SINGLE_FLOAT:
            return True

        if raw_mode not in (State.STATE_ALL_BIT, State.STATE_SINGLE_BIT):
            return False

        device_type_raw = await redis.get(f"device:{unit_id}:type")
        device_type = (to_str(device_type_raw, "") or "").strip().lower()
        if not device_type:
            prefix = unit_id.split("-", 1)[0].strip().lower()
            if prefix in {"do", "di", "ao"}:
                device_type = prefix

        return device_type in {"do", "ao"}

    @staticmethod
    def _normalize_state_mode(raw_mode: int) -> State:
        """Collapse firmware-specific aliases to a canonical State enum."""
        if raw_mode == State.DIAG_DI_BIT_V2:
            return State.DIAG_DI_BIT
        return State(raw_mode)

    @staticmethod
    async def update_state(
        unit_id: str,
        hdr: PacketHeader,
        decoded: object,
    ) -> tuple[bool, DeviceStateEvent | None]:
        """
        Update Redis state for the device based on the packet header/mode and decoded payload.
        Returns:
            changed: bool       -> True if Redis was updated
            event: DeviceStateEvent | None
        """
        decoded = cast(_DecodedState, decoded)
        redis = cast(RedisHashClient, cast(object, RedisManager.get_instance()))
        changed = False
        packet_id = cast(int | None, cast(object, getattr(hdr, "packet_id", None)))

        if hdr.mode == State.STATE_ALL_BIT:
            current = await redis.get(f"device:{unit_id}:bitmask")
            new_val = str(decoded.bitmask)
            if to_str(current) != new_val:
                await redis.set(f"device:{unit_id}:bitmask", new_val)
                changed = True

        elif hdr.mode == State.STATE_SINGLE_BIT:
            current = await redis.get(f"device:{unit_id}:bitmask")
            mask_val = to_int(current)
            if mask_val is not None:
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

        elif hdr.mode == State.DIAG_AO_FLOAT:
            diag_key = f"device:{unit_id}:diag_ao"
            current_diag = await redis.hgetall(diag_key)
            new_mapping = {
                "valid_mask": str(decoded.valid_mask),
                "pending_mask": str(decoded.pending_mask),
                "fault_mask": str(decoded.fault_mask),
                "error_mask": str(decoded.error_mask),
            }
            if any(current_diag.get(k) != v for k, v in new_mapping.items()):
                await redis.hset(diag_key, mapping=new_mapping)
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
            current_mask = to_int(current)
            if current_mask is not None:
                changed_mask = int(decoded.changed) & 0xFFFFFFFF
                state_mask = int(decoded.state) & 0xFFFFFFFF
                next_mask = (current_mask & ~changed_mask) | (state_mask & changed_mask)
                if next_mask != current_mask:
                    await redis.set(bitmask_key, str(next_mask))
                    changed = True

        elif hdr.mode in (State.DIAG_DI_BIT, State.DIAG_DI_BIT_V2):
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

        if packet_id is not None:
            await redis.set(f"device:{unit_id}:last_state_packet_id", str(packet_id))

        if not changed and not await DeviceStateService._should_emit_unchanged_event(unit_id, hdr.mode, redis):
            return False, None

        event = DeviceStateEvent(
            unit_id=unit_id,
            timestamp=hdr.timestamp_ms or int(time.time() * 1000),
            mode=DeviceStateService._normalize_state_mode(hdr.mode),
            payload=decoded.model_dump(),
        )
        return changed, event

    # -------------------------------------------------------------------------

    @staticmethod
    async def build_event_from_redis(unit_id: str, mode: State, ch: int | None = None) -> DeviceStateEvent:
        """
        Build a DeviceStateEvent from current values stored in Redis (no writes).
        Useful for snapshots or forced resend.
        """
        redis = cast(RedisHashClient, cast(object, RedisManager.get_instance()))

        if mode == State.STATE_ALL_BIT:
            bitmask = to_int(await redis.get(f"device:{unit_id}:bitmask"), 0)
            payload = {"bitmask": bitmask}

        elif mode == State.STATE_SINGLE_BIT:
            if ch is None:
                raise ValueError("Channel is required for a single-bit state event")
            bitmask = to_int(await redis.get(f"device:{unit_id}:bitmask"), 0)
            value = 1 if ((bitmask or 0) & (1 << ch)) else 0
            payload = {"ch": ch, "value": value}

        elif mode == State.STATE_SINGLE_FLOAT:
            val = await redis.hget(f"device:{unit_id}:ao", str(ch))
            payload = {"ch": ch, "value": float(to_str(val) or 0)}

        elif mode == State.DIAG_AO_FLOAT:
            diag = await redis.hgetall(f"device:{unit_id}:diag_ao")
            payload = {
                "valid_mask": to_int(diag.get("valid_mask"), 0),
                "pending_mask": to_int(diag.get("pending_mask"), 0),
                "fault_mask": to_int(diag.get("fault_mask"), 0),
                "error_mask": to_int(diag.get("error_mask"), 0),
            }

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
        - AO diagnostics (if available)
        - All AO channels
        """
        redis = cast(RedisHashClient, cast(object, RedisManager.get_instance()))
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

        ao_diag_exists = await redis.hlen(f"device:{unit_id}:diag_ao")
        if ao_diag_exists:
            events.append(await DeviceStateService.build_event_from_redis(unit_id, State.DIAG_AO_FLOAT))

        # Include AO channels if exist
        ao_channels = await redis.hkeys(f"device:{unit_id}:ao")
        for ch in ao_channels:
            events.append(
                await DeviceStateService.build_event_from_redis(unit_id, State.STATE_SINGLE_FLOAT, int(ch))
            )

        return events
