from __future__ import annotations

from dataclasses import dataclass
from datetime import datetime, timedelta, timezone
from typing import Any, Awaitable, Callable, Iterable, Sequence

from sqlalchemy import Select, delete, select
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy.orm import selectinload

from app.core.config import get_settings
from app.models.channel import Channel
from app.models.signal import Signal, SignalIODirection
from app.models.signal_sheet import SignalAllocation, SignalSheet, SignalSheetPreset
from app.models.workspace import Workspace
from app.schemas.signal_import_schema import SignalImportMetaSchema
from app.schemas.signal_sheet_schema import SignalAllocationRowSchema

settings = get_settings()


@dataclass(frozen=True)
class SignalSheetAutoAllocateResult:
    assigned: int
    skipped: int
    missing: int
    unassigned_signal_ids: list[int]
    changed_signal_ids: list[int]


_DIRECTION_TO_CHANNEL_TYPE: dict[str, str] = {
    "DI": "do",
    "DO": "di",
    "AI": "ao",
    "AO": "ai",
}


class SignalSheetRepository:
    def __init__(self, db: AsyncSession):
        self.db = db

    async def ensure_workspace(self, workspace_id: int) -> bool:
        stmt = select(Workspace.id).where(Workspace.id == workspace_id)
        result = await self.db.execute(stmt.limit(1))
        return result.scalar_one_or_none() is not None

    async def get_sheet(self, workspace_id: int) -> SignalSheet | None:
        stmt = select(SignalSheet).where(SignalSheet.workspace_id == workspace_id)
        result = await self.db.execute(stmt.limit(1))
        return result.scalar_one_or_none()

    async def upsert_sheet(
        self,
        *,
        workspace_id: int,
        source_filename: str | None,
        source_hash: str | None,
        rows_count: int,
        schema_version: int,
        data: dict[str, Any],
        import_meta: dict[str, Any] | None,
    ) -> SignalSheet:
        sheet = await self.get_sheet(workspace_id)
        if sheet is None:
            sheet = SignalSheet(
                workspace_id=workspace_id,
                source_filename=source_filename,
                source_hash=source_hash,
                rows_count=rows_count,
                schema_version=schema_version,
                data=data,
                import_meta=import_meta,
            )
            self.db.add(sheet)
            await self.db.flush()
            return sheet

        sheet.source_filename = source_filename
        sheet.source_hash = source_hash
        sheet.rows_count = rows_count
        sheet.schema_version = schema_version
        sheet.data = data
        sheet.import_meta = import_meta
        await self.db.flush()
        return sheet

    async def list_presets(self, workspace_id: int) -> list[SignalSheetPreset]:
        stmt = (
            select(SignalSheetPreset)
            .where(SignalSheetPreset.workspace_id == workspace_id)
            .order_by(SignalSheetPreset.updated_at.desc(), SignalSheetPreset.id.desc())
        )
        rows = await self.db.execute(stmt)
        return list(rows.scalars().all())

    async def get_preset(self, preset_id: int) -> SignalSheetPreset | None:
        stmt = select(SignalSheetPreset).where(SignalSheetPreset.id == preset_id)
        rows = await self.db.execute(stmt.limit(1))
        return rows.scalar_one_or_none()

    async def save_preset(
        self,
        *,
        workspace_id: int,
        name: str,
        import_meta: SignalImportMetaSchema,
    ) -> SignalSheetPreset:
        normalized_name = name.strip()
        if not normalized_name:
            raise ValueError("Preset name is required")

        stmt = select(SignalSheetPreset).where(
            SignalSheetPreset.workspace_id == workspace_id,
            SignalSheetPreset.name == normalized_name,
        )
        result = await self.db.execute(stmt.limit(1))
        preset = result.scalar_one_or_none()

        payload = import_meta.model_dump()
        if preset is None:
            preset = SignalSheetPreset(
                workspace_id=workspace_id,
                name=normalized_name,
                import_meta=payload,
            )
            self.db.add(preset)
        else:
            preset.import_meta = payload

        await self.db.commit()
        await self.db.refresh(preset)
        return preset

    async def delete_preset(self, preset_id: int) -> bool:
        preset = await self.get_preset(preset_id)
        if preset is None:
            return False
        await self.db.delete(preset)
        await self.db.commit()
        return True

    async def cleanup_orphan_allocations(self, workspace_id: int) -> None:
        stmt = (
            select(SignalAllocation)
            .options(selectinload(SignalAllocation.signal))
            .where(SignalAllocation.workspace_id == workspace_id)
        )
        allocations = list((await self.db.execute(stmt)).scalars().all())
        removed = False
        for allocation in allocations:
            signal = allocation.signal
            if signal is None:
                await self.db.delete(allocation)
                removed = True
                continue
            if signal.workspace_id != workspace_id or signal.deleted_at is not None or not signal.is_active:
                await self.db.delete(allocation)
                removed = True
        if removed:
            await self.db.flush()

    async def clear_allocations(self, workspace_id: int) -> None:
        stmt = delete(SignalAllocation).where(SignalAllocation.workspace_id == workspace_id)
        await self.db.execute(stmt)
        await self.db.flush()

    async def list_allocation_rows(self, workspace_id: int) -> list[SignalAllocationRowSchema]:
        signals = await self._list_active_signals(workspace_id)
        allocations_by_signal = await self._allocations_by_signal_id(workspace_id)
        return await self._build_allocation_rows(signals, allocations_by_signal)

    async def list_allocation_rows_page(
        self,
        workspace_id: int,
        *,
        offset: int,
        limit: int,
    ) -> list[SignalAllocationRowSchema]:
        if limit <= 0:
            return []
        signals = await self._list_active_signals_page(workspace_id, offset=offset, limit=limit)
        if not signals:
            return []
        signal_ids = {signal.id for signal in signals}
        allocations_by_signal = await self._allocations_by_signal_ids(workspace_id, signal_ids)
        return await self._build_allocation_rows(signals, allocations_by_signal)

    async def list_allocation_rows_by_signal_ids(
        self,
        workspace_id: int,
        signal_ids: Sequence[int],
    ) -> list[SignalAllocationRowSchema]:
        normalized_signal_ids = {int(signal_id) for signal_id in signal_ids}
        if not normalized_signal_ids:
            return []
        signals_by_id = await self._active_signals_by_ids(workspace_id, normalized_signal_ids)
        if not signals_by_id:
            return []
        signals = list(signals_by_id.values())
        signals.sort(key=lambda signal: signal.id)
        allocations_by_signal = await self._allocations_by_signal_ids(workspace_id, normalized_signal_ids)
        return await self._build_allocation_rows(signals, allocations_by_signal)

    async def _build_allocation_rows(
        self,
        signals: Sequence[Signal],
        allocations_by_signal: dict[int, SignalAllocation],
    ) -> list[SignalAllocationRowSchema]:
        channel_ids = {allocation.channel_id for allocation in allocations_by_signal.values()}
        channels_by_id = await self._channels_by_ids(channel_ids)

        rows: list[SignalAllocationRowSchema] = []
        for signal in signals:
            allocation = allocations_by_signal.get(signal.id)
            channel = channels_by_id.get(allocation.channel_id) if allocation else None
            unit_online = _is_unit_online(channel.device.last_seen_at) if channel and channel.device else None
            unit_last_seen_at = channel.device.last_seen_at if channel and channel.device else None

            rows.append(
                SignalAllocationRowSchema(
                    signal_id=signal.id,
                    signal_key=signal.key,
                    signal_name=signal.name,
                    signal_direction=_normalize_direction(signal.io_direction),
                    signal_category=signal.category,
                    signal_metadata=dict(signal.signal_metadata or {}),
                    channel_id=allocation.channel_id if allocation else None,
                    channel_type=channel.channel_type if channel else None,
                    channel_index=channel.channel_index if channel else None,
                    channel_label=channel.resolved_name if channel else None,
                    device_id=channel.device_id if channel else None,
                    unit_id=channel.device.unit_id if channel and channel.device else None,
                    unit_online=unit_online,
                    unit_last_seen_at=unit_last_seen_at,
                    tested_at=signal.tested_at,
                )
            )
        return rows

    async def update_allocations(
        self,
        workspace_id: int,
        entries: Sequence[dict[str, Any]],
        progress_callback: Callable[[int, int], Awaitable[None]] | None = None,
    ) -> None:
        await self.cleanup_orphan_allocations(workspace_id)

        if not entries:
            await self.db.commit()
            return

        signal_ids = {int(item["signal_id"]) for item in entries}
        active_signals = await self._active_signals_by_ids(workspace_id, signal_ids)
        if set(active_signals.keys()) != signal_ids:
            missing = sorted(signal_ids - set(active_signals.keys()))
            raise ValueError(f"Unknown or inactive signal_id values: {missing}")

        current_allocations = await self._allocations_by_signal_id(workspace_id)
        desired_channel_by_signal = {signal_id: allocation.channel_id for signal_id, allocation in current_allocations.items()}

        touched_meta: dict[int, dict[str, Any] | None] = {}
        for item in entries:
            signal_id = int(item["signal_id"])
            channel_id_raw = item.get("channel_id")
            if channel_id_raw is None:
                desired_channel_by_signal.pop(signal_id, None)
                touched_meta[signal_id] = item.get("allocation_meta")
                continue
            channel_id = int(channel_id_raw)
            desired_channel_by_signal[signal_id] = channel_id
            touched_meta[signal_id] = item.get("allocation_meta")

        desired_channel_ids = set(desired_channel_by_signal.values())
        channels_by_id = await self._channels_by_ids(desired_channel_ids)
        if set(channels_by_id.keys()) != desired_channel_ids:
            missing_channels = sorted(desired_channel_ids - set(channels_by_id.keys()))
            raise ValueError(f"Unknown channel_id values: {missing_channels}")

        seen_channel_owner: dict[int, int] = {}
        for signal_id, channel_id in desired_channel_by_signal.items():
            owner = seen_channel_owner.get(channel_id)
            if owner is not None and owner != signal_id:
                raise ValueError(f"Channel #{channel_id} is already allocated to another signal")
            seen_channel_owner[channel_id] = signal_id

            signal = active_signals.get(signal_id)
            if signal is None:
                signal = await self._signal_by_id(signal_id)
            if signal is None:
                raise ValueError(f"Signal #{signal_id} not found")
            channel = channels_by_id.get(channel_id)
            if channel is None:
                raise ValueError(f"Channel #{channel_id} not found")
            if not _is_channel_compatible(signal.io_direction, channel.channel_type):
                raise ValueError(
                    f"Channel #{channel_id} ({channel.channel_type}) is incompatible with signal #{signal_id} ({_normalize_direction(signal.io_direction)})"
                )

        touched_signal_ids = {int(item["signal_id"]) for item in entries}
        total_steps = len(touched_signal_ids)
        step_index = 0
        for signal_id in touched_signal_ids:
            step_index += 1
            desired_channel = desired_channel_by_signal.get(signal_id)
            existing = current_allocations.get(signal_id)
            if desired_channel is None:
                if existing is not None:
                    await self.db.delete(existing)
                if progress_callback is not None:
                    await progress_callback(step_index, total_steps)
                continue

            allocation_meta = touched_meta.get(signal_id)
            if existing is None:
                self.db.add(
                    SignalAllocation(
                        workspace_id=workspace_id,
                        signal_id=signal_id,
                        channel_id=desired_channel,
                        allocation_meta=allocation_meta,
                    )
                )
                if progress_callback is not None:
                    await progress_callback(step_index, total_steps)
                continue

            existing.channel_id = desired_channel
            if signal_id in touched_meta:
                existing.allocation_meta = allocation_meta
            if progress_callback is not None:
                await progress_callback(step_index, total_steps)

        await self.db.commit()

    async def mark_signals_tested(
        self,
        workspace_id: int,
        signal_ids: Sequence[int],
    ) -> list[int]:
        normalized_signal_ids = sorted({int(signal_id) for signal_id in signal_ids if int(signal_id) > 0})
        if not normalized_signal_ids:
            return []

        signals_by_id = await self._active_signals_by_ids(workspace_id, set(normalized_signal_ids))
        if not signals_by_id:
            return []

        tested_at_dt = datetime.now(timezone.utc)
        touched_ids: list[int] = []
        for signal_id in normalized_signal_ids:
            signal = signals_by_id.get(signal_id)
            if signal is None:
                continue
            signal.tested_at = tested_at_dt
            touched_ids.append(signal.id)

        if not touched_ids:
            return []

        await self.db.commit()
        return touched_ids

    async def mark_signals_tested_at(
        self,
        workspace_id: int,
        tested_at_by_signal: dict[int, str],
    ) -> list[int]:
        normalized = {
            int(signal_id): str(tested_at)
            for signal_id, tested_at in tested_at_by_signal.items()
            if int(signal_id) > 0 and str(tested_at).strip()
        }
        if not normalized:
            return []

        signals_by_id = await self._active_signals_by_ids(workspace_id, set(normalized.keys()))
        if not signals_by_id:
            return []

        touched_ids: list[int] = []
        for signal_id, tested_at in normalized.items():
            signal = signals_by_id.get(signal_id)
            if signal is None:
                continue
            tested_at_dt = _parse_tested_at_value(tested_at)
            if tested_at_dt is None:
                continue
            signal.tested_at = tested_at_dt
            touched_ids.append(signal.id)

        if not touched_ids:
            return []

        await self.db.commit()
        return touched_ids

    async def auto_allocate(
        self,
        *,
        workspace_id: int,
        signal_ids: Sequence[int] | None,
        prefer_online: bool,
        prefer_single_unit: bool,
        overwrite_existing: bool,
        progress_callback: Callable[[int, int], Awaitable[None]] | None = None,
    ) -> SignalSheetAutoAllocateResult:
        await self.cleanup_orphan_allocations(workspace_id)

        target_signals = await self._resolve_auto_allocate_targets(workspace_id, signal_ids)
        current_allocations = await self._allocations_by_signal_id(workspace_id)

        all_channels = await self._list_channels()
        channel_groups: dict[str, list[Channel]] = {
            "di": [],
            "do": [],
            "ai": [],
            "ao": [],
        }
        for channel in all_channels:
            key = _normalize_channel_type(channel.channel_type)
            if key is None:
                continue
            channel_groups[key].append(channel)

        for channels in channel_groups.values():
            channels.sort(
                key=lambda channel: _channel_auto_allocate_sort_key(
                    channel,
                    prefer_online=prefer_online,
                )
            )

        used_channel_ids = {allocation.channel_id for allocation in current_allocations.values()}
        changed_signal_ids: set[int] = set()

        assigned = 0
        skipped = 0
        missing = 0
        unassigned: list[int] = []

        preferred_unit_by_channel_type: dict[str, str] = {}
        if prefer_single_unit:
            preferred_unit_by_channel_type = _resolve_preferred_units_for_auto_allocate(
                target_signals=target_signals,
                current_allocations=current_allocations,
                channel_groups=channel_groups,
                used_channel_ids=used_channel_ids,
                overwrite_existing=overwrite_existing,
            )

        total_steps = len(target_signals)
        step_index = 0
        for signal in target_signals:
            step_index += 1
            direction = _normalize_direction(signal.io_direction)
            required_channel_type = _required_channel_type(direction)
            if required_channel_type is None:
                missing += 1
                unassigned.append(signal.id)
                if progress_callback is not None:
                    await progress_callback(step_index, total_steps)
                continue

            existing = current_allocations.get(signal.id)
            if existing is not None and not overwrite_existing:
                skipped += 1
                if progress_callback is not None:
                    await progress_callback(step_index, total_steps)
                continue

            if existing is not None and overwrite_existing:
                used_channel_ids.discard(existing.channel_id)

            preferred_unit_id = preferred_unit_by_channel_type.get(required_channel_type)
            candidate = _pick_candidate_channel(
                candidates=channel_groups.get(required_channel_type, []),
                used_channel_ids=used_channel_ids,
                preferred_unit_id=preferred_unit_id,
                prefer_online=prefer_online,
                allow_offline_fallback=not (prefer_online and preferred_unit_id is not None),
            )
            if candidate is None and preferred_unit_id is not None:
                candidate = _pick_candidate_channel(
                    candidates=channel_groups.get(required_channel_type, []),
                    used_channel_ids=used_channel_ids,
                    preferred_unit_id=None,
                    prefer_online=prefer_online,
                )
            if candidate is None:
                unassigned.append(signal.id)
                if existing is not None and overwrite_existing:
                    used_channel_ids.add(existing.channel_id)
                if progress_callback is not None:
                    await progress_callback(step_index, total_steps)
                continue

            if existing is None:
                allocation = SignalAllocation(
                    workspace_id=workspace_id,
                    signal_id=signal.id,
                    channel_id=candidate.id,
                    allocation_meta={"source": "auto"},
                )
                self.db.add(allocation)
                current_allocations[signal.id] = allocation
                changed_signal_ids.add(signal.id)
            else:
                previous_channel_id = existing.channel_id
                existing.channel_id = candidate.id
                meta = dict(existing.allocation_meta or {})
                meta["source"] = "auto"
                existing.allocation_meta = meta
                if previous_channel_id != candidate.id:
                    changed_signal_ids.add(signal.id)

            used_channel_ids.add(candidate.id)
            assigned += 1
            if progress_callback is not None:
                await progress_callback(step_index, total_steps)

        await self.db.commit()
        return SignalSheetAutoAllocateResult(
            assigned=assigned,
            skipped=skipped,
            missing=missing,
            unassigned_signal_ids=sorted(set(unassigned)),
            changed_signal_ids=sorted(changed_signal_ids),
        )

    async def count_active_signals(self, workspace_id: int) -> int:
        stmt = select(Signal.id).where(
            Signal.workspace_id == workspace_id,
            Signal.deleted_at.is_(None),
            Signal.is_active.is_(True),
        )
        rows = await self.db.execute(stmt)
        return len(rows.scalars().all())

    async def count_allocated_signals(self, workspace_id: int) -> int:
        stmt = select(SignalAllocation.id).where(SignalAllocation.workspace_id == workspace_id)
        rows = await self.db.execute(stmt)
        return len(rows.scalars().all())

    async def _list_active_signals(self, workspace_id: int) -> list[Signal]:
        stmt = (
            select(Signal)
            .where(
                Signal.workspace_id == workspace_id,
                Signal.deleted_at.is_(None),
                Signal.is_active.is_(True),
            )
            .order_by(Signal.created_at.asc(), Signal.id.asc())
        )
        rows = await self.db.execute(stmt)
        return list(rows.scalars().all())

    async def _list_active_signals_page(
        self,
        workspace_id: int,
        *,
        offset: int,
        limit: int,
    ) -> list[Signal]:
        normalized_offset = max(0, int(offset))
        normalized_limit = max(0, int(limit))
        if normalized_limit <= 0:
            return []
        stmt = (
            select(Signal)
            .where(
                Signal.workspace_id == workspace_id,
                Signal.deleted_at.is_(None),
                Signal.is_active.is_(True),
            )
            .order_by(Signal.created_at.asc(), Signal.id.asc())
            .offset(normalized_offset)
            .limit(normalized_limit)
        )
        rows = await self.db.execute(stmt)
        return list(rows.scalars().all())

    async def _active_signals_by_ids(self, workspace_id: int, signal_ids: set[int]) -> dict[int, Signal]:
        if not signal_ids:
            return {}
        stmt = (
            select(Signal)
            .where(
                Signal.workspace_id == workspace_id,
                Signal.deleted_at.is_(None),
                Signal.is_active.is_(True),
                Signal.id.in_(signal_ids),
            )
        )
        rows = await self.db.execute(stmt)
        return {signal.id: signal for signal in rows.scalars().all()}

    async def _signal_by_id(self, signal_id: int) -> Signal | None:
        stmt = select(Signal).where(Signal.id == signal_id)
        rows = await self.db.execute(stmt.limit(1))
        return rows.scalar_one_or_none()

    async def _allocations_by_signal_id(self, workspace_id: int) -> dict[int, SignalAllocation]:
        stmt: Select[tuple[SignalAllocation]] = (
            select(SignalAllocation)
            .join(Signal, SignalAllocation.signal_id == Signal.id)
            .where(
                SignalAllocation.workspace_id == workspace_id,
                Signal.workspace_id == workspace_id,
                Signal.deleted_at.is_(None),
                Signal.is_active.is_(True),
            )
            .options(selectinload(SignalAllocation.signal))
        )
        rows = await self.db.execute(stmt)
        allocations = list(rows.scalars().all())
        return {item.signal_id: item for item in allocations}

    async def _allocations_by_signal_ids(
        self,
        workspace_id: int,
        signal_ids: set[int],
    ) -> dict[int, SignalAllocation]:
        if not signal_ids:
            return {}
        stmt: Select[tuple[SignalAllocation]] = (
            select(SignalAllocation)
            .join(Signal, SignalAllocation.signal_id == Signal.id)
            .where(
                SignalAllocation.workspace_id == workspace_id,
                Signal.workspace_id == workspace_id,
                Signal.deleted_at.is_(None),
                Signal.is_active.is_(True),
                SignalAllocation.signal_id.in_(signal_ids),
            )
            .options(selectinload(SignalAllocation.signal))
        )
        rows = await self.db.execute(stmt)
        allocations = list(rows.scalars().all())
        return {item.signal_id: item for item in allocations}

    async def _channels_by_ids(self, channel_ids: set[int]) -> dict[int, Channel]:
        if not channel_ids:
            return {}
        stmt = (
            select(Channel)
            .where(Channel.id.in_(channel_ids))
            .options(selectinload(Channel.device))
        )
        rows = await self.db.execute(stmt)
        return {channel.id: channel for channel in rows.scalars().all()}

    async def _list_channels(self) -> list[Channel]:
        stmt = select(Channel).options(selectinload(Channel.device)).order_by(Channel.id.asc())
        rows = await self.db.execute(stmt)
        return list(rows.scalars().all())

    async def _resolve_auto_allocate_targets(
        self,
        workspace_id: int,
        signal_ids: Sequence[int] | None,
    ) -> list[Signal]:
        if signal_ids:
            normalized = [int(item) for item in signal_ids]
            signal_map = await self._active_signals_by_ids(workspace_id, set(normalized))
            payload: list[Signal] = []
            seen: set[int] = set()
            for signal_id in normalized:
                signal = signal_map.get(signal_id)
                if signal is None or signal.id in seen:
                    continue
                payload.append(signal)
                seen.add(signal.id)
            return payload

        return await self._list_active_signals(workspace_id)


def _normalize_direction(value: SignalIODirection | str | None) -> str:
    if value is None:
        return ""
    if isinstance(value, SignalIODirection):
        return value.value
    return str(value).strip().upper()


def _required_channel_type(direction: str) -> str | None:
    return _DIRECTION_TO_CHANNEL_TYPE.get(direction.strip().upper())


def _normalize_channel_type(value: str | None) -> str | None:
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


def _is_channel_compatible(direction: SignalIODirection | str | None, channel_type: str | None) -> bool:
    required = _required_channel_type(_normalize_direction(direction))
    if required is None:
        return False
    return _normalize_channel_type(channel_type) == required


def _is_unit_online(last_seen_at: datetime | None) -> bool:
    if last_seen_at is None:
        return False
    heartbeat_ttl_seconds = max(int(settings.heartbeat_ttl or 30), 10)
    threshold = datetime.now(timezone.utc) - timedelta(seconds=heartbeat_ttl_seconds * 2)
    return last_seen_at >= threshold


def _parse_tested_at_value(raw_value: Any) -> datetime | None:
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


def _pick_candidate_channel(
    *,
    candidates: Iterable[Channel],
    used_channel_ids: set[int],
    preferred_unit_id: str | None = None,
    prefer_online: bool = True,
    allow_offline_fallback: bool = True,
) -> Channel | None:
    if prefer_online:
        for channel in candidates:
            if channel.id in used_channel_ids:
                continue
            if preferred_unit_id is not None:
                unit_id = channel.device.unit_id if channel.device else None
                if unit_id != preferred_unit_id:
                    continue
            if not _is_unit_online(channel.device.last_seen_at if channel.device else None):
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


def _channel_auto_allocate_sort_key(
    channel: Channel,
    *,
    prefer_online: bool,
) -> tuple[int, int, int, int]:
    online_rank = 0
    if prefer_online:
        online_rank = 0 if _is_unit_online(channel.device.last_seen_at if channel.device else None) else 1
    return (
        online_rank,
        int(channel.device_id),
        int(channel.channel_index),
        int(channel.id),
    )


def _resolve_preferred_units_for_auto_allocate(
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

    for required_channel_type in ("di", "do", "ai", "ao"):
        required_count = 0
        existing_unit_weights: dict[str, int] = {}

        for signal in target_signals:
            mapped_type = _required_channel_type(_normalize_direction(signal.io_direction))
            if mapped_type != required_channel_type:
                continue

            existing = current_allocations.get(signal.id)
            if existing is not None and not overwrite_existing:
                channel = next(
                    (candidate for candidate in channel_groups.get(required_channel_type, []) if candidate.id == existing.channel_id),
                    None,
                )
                unit_id = channel.device.unit_id if channel and channel.device else None
                if unit_id:
                    existing_unit_weights[unit_id] = existing_unit_weights.get(unit_id, 0) + 1
                continue

            required_count += 1

        if required_count <= 0:
            # Nothing new to allocate for this type, but if existing target allocations
            # already lean to one unit, keep that as preference.
            if existing_unit_weights:
                preferred_by_type[required_channel_type] = max(
                    existing_unit_weights.items(),
                    key=lambda item: item[1],
                )[0]
            continue

        if existing_unit_weights:
            preferred_by_type[required_channel_type] = max(
                existing_unit_weights.items(),
                key=lambda item: item[1],
            )[0]
            continue

        free_count_by_unit: dict[str, int] = {}
        ordered_units: list[str] = []
        for channel in channel_groups.get(required_channel_type, []):
            unit_id = channel.device.unit_id if channel.device else None
            if not unit_id:
                continue

            allocation_owner = allocation_owner_by_channel_id.get(channel.id)
            if channel.id in used_channel_ids:
                # Channel occupied by another signal and not being overwritten.
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
            preferred_by_type[required_channel_type] = preferred_unit

    return preferred_by_type
