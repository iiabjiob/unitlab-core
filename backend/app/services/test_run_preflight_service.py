from __future__ import annotations

from dataclasses import dataclass
from datetime import datetime, timedelta, timezone

from app.api.v1.test_runs import TestRunsRepository
from app.core.config import get_settings
from app.models.channel import Channel
from app.models.test_run import TestRun, TestRunAllocationEntry
from app.schemas.test_run_schema import (
    TestRunPreflightEntrySchema,
    TestRunPreflightSchema,
    TestRunPreflightUnitSchema,
)
from app.services.signal_binding_service import SignalBindingService

settings = get_settings()


@dataclass(frozen=True)
class _Availability:
    available: bool
    reason: str | None = None


class TestRunPreflightService:
    """Checks unit/channel availability and proposes channel reallocation candidates."""

    @staticmethod
    async def evaluate(repo: TestRunsRepository, run: TestRun) -> TestRunPreflightSchema:
        allocation_entries = run.allocation.entries if run.allocation else []
        channels_by_id = await repo.get_channels_by_ids({entry.channel_id for entry in allocation_entries})
        available_channels = await repo.list_channels()

        available_pool_by_direction: dict[str, list[int]] = {
            "DI": [],
            "DO": [],
            "AI": [],
            "AO": [],
        }

        for channel in available_channels:
            availability = TestRunPreflightService._channel_availability(channel)
            if not availability.available:
                continue
            direction = TestRunPreflightService._direction_for_channel_type(channel.channel_type)
            if direction:
                available_pool_by_direction.setdefault(direction, []).append(channel.id)

        units_map: dict[int, TestRunPreflightUnitSchema] = {}
        entries_payload: list[TestRunPreflightEntrySchema] = []
        used_channels = {entry.channel_id for entry in allocation_entries}

        for entry in allocation_entries:
            channel = channels_by_id.get(entry.channel_id)
            signal_key = SignalBindingService.extract_signal_key(entry)
            required_direction = TestRunPreflightService._required_direction(entry, channel)

            if channel is None:
                availability = _Availability(available=False, reason="channel_not_found")
            else:
                availability = TestRunPreflightService._channel_availability(channel)

            recommended = []
            if not availability.available and required_direction:
                candidates = available_pool_by_direction.get(required_direction, [])
                recommended = [channel_id for channel_id in candidates if channel_id not in used_channels][:5]

            if channel is not None and channel.device is not None:
                device = channel.device
                unit_status = TestRunPreflightService._device_availability(device.last_seen_at)
                units_map[device.id] = TestRunPreflightUnitSchema(
                    device_id=device.id,
                    unit_id=device.unit_id,
                    available=unit_status.available,
                    last_seen_at=device.last_seen_at,
                    reason=unit_status.reason,
                )

            entries_payload.append(
                TestRunPreflightEntrySchema(
                    allocation_entry_id=entry.id,
                    channel_id=entry.channel_id,
                    signal_id=entry.signal_id,
                    signal_key=signal_key,
                    required_direction=required_direction,
                    available=availability.available,
                    reason=availability.reason,
                    recommended_channel_ids=recommended,
                )
            )

        ready = bool(entries_payload) and all(item.available for item in entries_payload)
        reallocation_required = any(not item.available for item in entries_payload)

        return TestRunPreflightSchema(
            test_run_id=run.id,
            ready=ready,
            reallocation_required=reallocation_required,
            units=sorted(units_map.values(), key=lambda item: item.device_id),
            entries=entries_payload,
        )

    @staticmethod
    def _required_direction(entry: TestRunAllocationEntry, channel: Channel | None) -> str | None:
        if entry.signal is not None:
            value = entry.signal.io_direction
            return value if isinstance(value, str) else value.value
        if channel is not None:
            return TestRunPreflightService._direction_for_channel_type(channel.channel_type)
        return None

    @staticmethod
    def _channel_availability(channel: Channel) -> _Availability:
        if channel.device is None:
            return _Availability(False, "device_missing")
        return TestRunPreflightService._device_availability(channel.device.last_seen_at)

    @staticmethod
    def _device_availability(last_seen_at: datetime | None) -> _Availability:
        if last_seen_at is None:
            return _Availability(False, "never_seen")

        heartbeat_ttl_seconds = max(int(settings.heartbeat_ttl or 30), 10)
        threshold = datetime.now(timezone.utc) - timedelta(seconds=heartbeat_ttl_seconds * 2)
        if last_seen_at < threshold:
            return _Availability(False, "offline")
        return _Availability(True, None)

    @staticmethod
    def _direction_for_channel_type(channel_type: str | None) -> str | None:
        if not channel_type:
            return None
        normalized = str(channel_type).strip().lower()
        if normalized.startswith("di"):
            return "DI"
        if normalized.startswith("do"):
            return "DO"
        if normalized.startswith("ai"):
            return "AI"
        if normalized.startswith("ao"):
            return "AO"
        return None
