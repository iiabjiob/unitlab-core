from __future__ import annotations

from datetime import datetime, timezone
from typing import Iterable

from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy.orm import selectinload

from app.models.channel import Channel
from app.models.sequence import Sequence
from app.models.signal import Signal, SignalIODirection
from app.models.test_run import (
    TestRun,
    TestRunAllocation,
    TestRunAllocationEntry,
    TestRunSequence,
    TestRunSignalSnapshot,
    TestRunSignalSnapshotEntry,
    TestRunStatus,
)
from app.models.workspace import Workspace, WorkspaceSequence


class TestRunsRepository:
    def __init__(self, db: AsyncSession):
        self.db = db

    async def ensure_workspace(self, workspace_id: int) -> bool:
        stmt = select(Workspace.id).where(Workspace.id == workspace_id)
        result = await self.db.execute(stmt.limit(1))
        return result.scalar_one_or_none() is not None

    async def list(self, workspace_id: int) -> list[TestRun]:
        stmt = (
            select(TestRun)
            .options(
                selectinload(TestRun.sequence_links),
                selectinload(TestRun.allocation)
                .selectinload(TestRunAllocation.entries)
                .selectinload(TestRunAllocationEntry.signal),
                selectinload(TestRun.allocation)
                .selectinload(TestRunAllocation.entries)
                .selectinload(TestRunAllocationEntry.channel)
                .selectinload(Channel.device),
                selectinload(TestRun.snapshot),
            )
            .where(TestRun.workspace_id == workspace_id)
            .order_by(TestRun.created_at.desc())
        )
        result = await self.db.execute(stmt)
        return list(result.scalars().all())

    async def get(self, run_id: int) -> TestRun | None:
        stmt = (
            select(TestRun)
            .options(
                selectinload(TestRun.sequence_links),
                selectinload(TestRun.allocation)
                .selectinload(TestRunAllocation.entries)
                .selectinload(TestRunAllocationEntry.signal),
                selectinload(TestRun.allocation)
                .selectinload(TestRunAllocation.entries)
                .selectinload(TestRunAllocationEntry.channel)
                .selectinload(Channel.device),
                selectinload(TestRun.snapshot),
            )
            .where(TestRun.id == run_id)
        )
        result = await self.db.execute(stmt)
        return result.scalar_one_or_none()

    async def ensure_sequences_in_workspace(self, workspace_id: int, sequence_ids: Iterable[int]) -> bool:
        unique_ids = list({int(seq_id) for seq_id in sequence_ids})
        if not unique_ids:
            return False

        stmt = (
            select(WorkspaceSequence.sequence_id)
            .where(
                WorkspaceSequence.workspace_id == workspace_id,
                WorkspaceSequence.sequence_id.in_(unique_ids),
            )
        )
        rows = await self.db.execute(stmt)
        existing = set(rows.scalars().all())
        return existing == set(unique_ids)

    async def ensure_channels_exist(self, channel_ids: Iterable[int]) -> bool:
        unique_ids = {int(channel_id) for channel_id in channel_ids}
        if not unique_ids:
            return True

        stmt = select(Channel.id).where(Channel.id.in_(unique_ids))
        rows = await self.db.execute(stmt)
        return set(rows.scalars().all()) == unique_ids

    async def ensure_signals_in_workspace(self, workspace_id: int, signal_ids: Iterable[int]) -> bool:
        unique_ids = {int(signal_id) for signal_id in signal_ids}
        if not unique_ids:
            return True

        stmt = select(Signal.id).where(
            Signal.workspace_id == workspace_id,
            Signal.id.in_(unique_ids),
            Signal.deleted_at.is_(None),
        )
        rows = await self.db.execute(stmt)
        return set(rows.scalars().all()) == unique_ids

    async def create(
        self,
        *,
        workspace_id: int,
        sequence_ids: list[int],
        allocation_notes: str | None,
        allocation_entries: list[dict],
        status: TestRunStatus = TestRunStatus.CREATED,
        source_test_run_id: int | None = None,
        allocation_revision: int = 1,
    ) -> TestRun:
        run = TestRun(
            workspace_id=workspace_id,
            source_test_run_id=source_test_run_id,
            allocation_revision=allocation_revision,
            status=status,
        )
        self.db.add(run)
        await self.db.flush()

        run.sequence_links = [
            TestRunSequence(test_run_id=run.id, sequence_id=sequence_id, order_index=index)
            for index, sequence_id in enumerate(sequence_ids)
        ]

        allocation = TestRunAllocation(test_run_id=run.id, notes=allocation_notes)
        allocation.entries = [
            TestRunAllocationEntry(
                channel_id=int(entry["channel_id"]),
                signal_id=int(entry["signal_id"]) if entry.get("signal_id") is not None else None,
                signal_metadata=entry.get("signal_metadata"),
            )
            for entry in allocation_entries
        ]
        run.allocation = allocation

        await self.db.commit()
        refreshed = await self.get(run.id)
        if refreshed is None:
            raise RuntimeError("Failed to reload created test run")
        return refreshed

    async def clone(self, source: TestRun) -> TestRun:
        sequence_ids = [link.sequence_id for link in sorted(source.sequence_links, key=lambda item: item.order_index)]
        entries_payload: list[dict] = []
        if source.allocation:
            for entry in source.allocation.entries:
                entries_payload.append(
                    {
                        "channel_id": entry.channel_id,
                        "signal_id": entry.signal_id,
                        "signal_metadata": dict(entry.signal_metadata or {}) if entry.signal_metadata else None,
                    }
                )

        notes = source.allocation.notes if source.allocation else None
        next_revision = int(source.allocation_revision or 1) + 1
        return await self.create(
            workspace_id=source.workspace_id,
            sequence_ids=sequence_ids,
            allocation_notes=notes,
            allocation_entries=entries_payload,
            status=TestRunStatus.CREATED,
            source_test_run_id=source.id,
            allocation_revision=next_revision,
        )

    async def get_channels_by_ids(self, channel_ids: set[int]) -> dict[int, Channel]:
        if not channel_ids:
            return {}
        stmt = (
            select(Channel)
            .options(selectinload(Channel.device))
            .where(Channel.id.in_(channel_ids))
        )
        rows = await self.db.execute(stmt)
        return {channel.id: channel for channel in rows.scalars().all()}

    async def list_channels(self) -> list[Channel]:
        stmt = select(Channel).options(selectinload(Channel.device)).order_by(Channel.id.asc())
        rows = await self.db.execute(stmt)
        return list(rows.scalars().all())

    async def create_signal_snapshot(self, run: TestRun) -> None:
        existing_snapshot = await self.get_signal_snapshot(run.id)
        if existing_snapshot is not None:
            await self.db.delete(existing_snapshot)
            await self.db.flush()

        snapshot = TestRunSignalSnapshot(
            test_run_id=run.id,
            workspace_id=run.workspace_id,
        )

        entries: list[TestRunSignalSnapshotEntry] = []
        seen_signal_keys: set[str] = set()
        allocation_entries = run.allocation.entries if run.allocation else []
        for allocation_entry in allocation_entries:
            signal = allocation_entry.signal
            metadata = dict(allocation_entry.signal_metadata or {}) if allocation_entry.signal_metadata else {}

            signal_key = signal.key if signal is not None and signal.key else self._extract_signal_key_from_meta(metadata)
            if not signal_key or signal_key in seen_signal_keys:
                continue

            direction = (
                signal.io_direction if signal is not None else self._extract_direction_from_meta(metadata)
            )
            if direction is None:
                continue

            seen_signal_keys.add(signal_key)
            entries.append(
                TestRunSignalSnapshotEntry(
                    snapshot_id=run.id,
                    live_signal_id=signal.id if signal is not None else None,
                    signal_key=signal_key,
                    name=signal.name if signal is not None else (self._extract_name_from_meta(metadata) or signal_key),
                    io_direction=direction,
                    allocation_channel_id=allocation_entry.channel_id,
                    allocation_metadata=metadata or None,
                    entry_metadata=dict(signal.signal_metadata or {}) if signal is not None else metadata,
                )
            )

        snapshot.entries = entries
        self.db.add(snapshot)
        await self.db.flush()

    async def get_signal_snapshot(self, run_id: int) -> TestRunSignalSnapshot | None:
        stmt = (
            select(TestRunSignalSnapshot)
            .options(selectinload(TestRunSignalSnapshot.entries))
            .where(TestRunSignalSnapshot.test_run_id == run_id)
        )
        result = await self.db.execute(stmt)
        return result.scalar_one_or_none()

    async def mark_running(self, run: TestRun, execution_meta: dict) -> None:
        run.status = TestRunStatus.RUNNING
        run.started_at = datetime.now(timezone.utc)
        run.finished_at = None
        run.execution_meta = execution_meta
        await self.db.flush()

    async def mark_failed(self, run: TestRun, execution_meta: dict | None = None) -> None:
        run.status = TestRunStatus.FAILED
        run.finished_at = datetime.now(timezone.utc)
        if execution_meta is not None:
            run.execution_meta = execution_meta
        await self.db.flush()

    async def mark_completed(self, run: TestRun, execution_meta: dict | None = None) -> None:
        run.status = TestRunStatus.COMPLETED
        run.finished_at = datetime.now(timezone.utc)
        if execution_meta is not None:
            run.execution_meta = execution_meta
        await self.db.flush()

    async def sequence_exists(self, sequence_id: int) -> bool:
        stmt = select(Sequence.id).where(Sequence.id == sequence_id)
        result = await self.db.execute(stmt.limit(1))
        return result.scalar_one_or_none() is not None

    @staticmethod
    def _extract_signal_key_from_meta(metadata: dict) -> str | None:
        for key_name in ("signal_key", "snapshot_signal_key", "key"):
            value = metadata.get(key_name)
            if isinstance(value, str) and value.strip():
                return value.strip()
        return None

    @staticmethod
    def _extract_name_from_meta(metadata: dict) -> str | None:
        for key_name in ("signal_name", "name", "snapshot_signal_name"):
            value = metadata.get(key_name)
            if isinstance(value, str) and value.strip():
                return value.strip()
        return None

    @staticmethod
    def _extract_direction_from_meta(metadata: dict) -> SignalIODirection | None:
        for key_name in ("signal_direction", "io_direction", "direction"):
            raw = metadata.get(key_name)
            if not isinstance(raw, str):
                continue
            normalized = raw.strip().upper()
            if not normalized:
                continue
            try:
                return SignalIODirection(normalized)
            except ValueError:
                continue
        return None
