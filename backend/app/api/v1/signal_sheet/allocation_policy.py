from __future__ import annotations

from datetime import datetime, timedelta, timezone
from typing import Any, Iterable, Sequence

from app.core.config import get_settings
from app.models.channel import Channel
from app.models.signal import Signal, SignalIODirection
from app.models.signal_sheet import SignalAllocation

settings = get_settings()

_DIRECTION_TO_CHANNEL_TYPE: dict[str, str] = {
    "DI": "do",
    "DO": "di",
    "AI": "ao",
    "AO": "ai",
}


def normalize_direction(value: SignalIODirection | str | None) -> str:
    if value is None:
        return ""
    if isinstance(value, SignalIODirection):
        return value.value
    return str(value).strip().upper()


def required_channel_type(direction: str) -> str | None:
    return _DIRECTION_TO_CHANNEL_TYPE.get(direction.strip().upper())


def normalize_channel_type(value: str | None) -> str | None:
    if value is None:
        return None
    normalized = str(value).strip().lower()
    if normalized.startswith("di"):
        return "di"
    if normalized.startswith("do"):
        return "do"
    if normalized.startswith("ai"):
        return "ai"
    if normalized.startswith("ao"):
        return "ao"
    return None


def is_channel_compatible(direction: SignalIODirection | str | None, channel_type: str | None) -> bool:
    required = required_channel_type(normalize_direction(direction))
    if required is None:
        return False
    return normalize_channel_type(channel_type) == required


def is_unit_online(last_seen_at: datetime | None) -> bool:
    if last_seen_at is None:
        return False
    heartbeat_ttl_seconds = max(int(settings.heartbeat_ttl or 30), 10)
    threshold = datetime.now(timezone.utc) - timedelta(seconds=heartbeat_ttl_seconds * 2)
    return last_seen_at >= threshold


def parse_tested_at_value(raw_value: Any) -> datetime | None:
    if raw_value is None:
        return None
    if isinstance(raw_value, datetime):
        return raw_value
    if isinstance(raw_value, str):
        candidate = raw_value.strip()
        if not candidate:
            return None
        if candidate.endswith("Z"):
            candidate = candidate[:-1] + "+00:00"
        try:
            return datetime.fromisoformat(candidate)
        except ValueError:
            return None
    return None


def pick_candidate_channel(
    *,
    candidates: Iterable[Channel],
    used_channel_ids: set[int],
    preferred_unit_id: str | None = None,
    prefer_online: bool = True,
    allow_offline_fallback: bool = True,
    online_by_unit_id: dict[str, bool] | None = None,
) -> Channel | None:
    if prefer_online:
        for channel in candidates:
            if channel.id in used_channel_ids:
                continue
            if preferred_unit_id is not None:
                unit_id = channel.device.unit_id if channel.device else None
                if unit_id != preferred_unit_id:
                    continue
            unit_id = channel.device.unit_id if channel.device else None
            if online_by_unit_id is not None and unit_id:
                is_online = bool(online_by_unit_id.get(unit_id, False))
            else:
                is_online = is_unit_online(channel.device.last_seen_at if channel.device else None)
            if not is_online:
                continue
            return channel

    if prefer_online and not allow_offline_fallback:
        return None

    for channel in candidates:
        if channel.id in used_channel_ids:
            continue
        if preferred_unit_id is not None:
            unit_id = channel.device.unit_id if channel.device else None
            if unit_id != preferred_unit_id:
                continue
        return channel
    return None


def channel_auto_allocate_sort_key(
    channel: Channel,
    *,
    prefer_online: bool,
    online_by_unit_id: dict[str, bool] | None = None,
) -> tuple[int, int, int, int]:
    online_rank = 0
    if prefer_online:
        unit_id = channel.device.unit_id if channel.device else None
        if online_by_unit_id is not None and unit_id:
            is_online = bool(online_by_unit_id.get(unit_id, False))
        else:
            is_online = is_unit_online(channel.device.last_seen_at if channel.device else None)
        online_rank = 0 if is_online else 1
    return (
        online_rank,
        int(channel.device_id),
        int(channel.channel_index),
        int(channel.id),
    )


def resolve_preferred_units_for_auto_allocate(
    *,
    target_signals: Sequence[Signal],
    current_allocations: dict[int, SignalAllocation],
    channel_groups: dict[str, list[Channel]],
    used_channel_ids: set[int],
    overwrite_existing: bool,
) -> dict[str, str]:
    # Prefer channels from a single unit per channel-type bucket ("di"/"do"/"ai"/"ao")
    # when there is enough free capacity.
    preferred_by_type: dict[str, str] = {}
    target_ids = {signal.id for signal in target_signals}
    allocation_owner_by_channel_id = {
        allocation.channel_id: signal_id
        for signal_id, allocation in current_allocations.items()
    }

    for required_type in ("di", "do", "ai", "ao"):
        required_count = 0
        existing_unit_weights: dict[str, int] = {}

        for signal in target_signals:
            mapped_type = required_channel_type(normalize_direction(signal.io_direction))
            if mapped_type != required_type:
                continue

            existing = current_allocations.get(signal.id)
            if existing is not None and not overwrite_existing:
                channel = next(
                    (candidate for candidate in channel_groups.get(required_type, []) if candidate.id == existing.channel_id),
                    None,
                )
                unit_id = channel.device.unit_id if channel and channel.device else None
                if unit_id:
                    existing_unit_weights[unit_id] = existing_unit_weights.get(unit_id, 0) + 1
                continue

            required_count += 1

        if required_count <= 0:
            if existing_unit_weights:
                preferred_by_type[required_type] = max(
                    existing_unit_weights.items(),
                    key=lambda item: item[1],
                )[0]
            continue

        if existing_unit_weights:
            preferred_by_type[required_type] = max(
                existing_unit_weights.items(),
                key=lambda item: item[1],
            )[0]
            continue

        free_count_by_unit: dict[str, int] = {}
        ordered_units: list[str] = []
        for channel in channel_groups.get(required_type, []):
            unit_id = channel.device.unit_id if channel.device else None
            if not unit_id:
                continue

            allocation_owner = allocation_owner_by_channel_id.get(channel.id)
            if channel.id in used_channel_ids:
                if allocation_owner is None or allocation_owner not in target_ids or not overwrite_existing:
                    continue
            if unit_id not in free_count_by_unit:
                free_count_by_unit[unit_id] = 0
                ordered_units.append(unit_id)
            free_count_by_unit[unit_id] += 1

        preferred_unit = next(
            (unit_id for unit_id in ordered_units if free_count_by_unit.get(unit_id, 0) >= required_count),
            None,
        )
        if preferred_unit is not None:
            preferred_by_type[required_type] = preferred_unit

    return preferred_by_type
