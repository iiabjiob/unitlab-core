"""Business logic for creating and repeating test runs."""
from __future__ import annotations

import copy
from datetime import datetime, timezone

from sqlalchemy import Select, select
from sqlalchemy.orm import selectinload
from sqlalchemy.ext.asyncio import AsyncSession

from app.models.allocation import Allocation, AllocationEntry
from app.models.channel import Channel
from app.models.signal import Signal
from app.models.sequence import Sequence
from app.models.test_run import TestRun, TestRunSequenceLink, TestRunStatus
from app.models.test_run_signal_snapshot import (
    TestRunSignalSnapshot,
    TestRunSignalSnapshotEntry,
)
from app.models.workspace import WorkspaceSequence
from app.schemas.allocation_schema import AllocationCreateSchema, AllocationEntryCreateSchema
from app.schemas.test_run_schema import TestRunCreateSchema
from app.services.execution_context import (
    AllocationEntryDTO,
    ExecutionContext,
    SequenceDTO,
    SequenceStepDTO,
)
from app.services.domain_errors import (
    AllocationInvalidError,
    ChannelNotAllocatedError,
    ChannelNotFoundError,
    SequenceNotApplicableError,
    TestRunInvalidStateError,
)


class WorkspaceResourceNotFoundError(Exception):
    """Raised when a workspace-scoped asset is missing."""


class DuplicateSequenceSelectionError(Exception):
    """Raised when run creation payload repeats the same sequence."""


class TestRunNotFoundError(Exception):
    """Raised when a TestRun record cannot be found."""


class TestRunAllocationMissingError(AllocationInvalidError):
    """Raised when a TestRun does not have an allocation."""


class TestRunSequencesMissingError(SequenceNotApplicableError):
    """Raised when a TestRun has no sequences attached."""


class TestRunService:
    def __init__(self, db: AsyncSession):
        self.db = db

    async def list_runs(self, workspace_id: int) -> list[TestRun]:
        stmt = (
            select(TestRun)
            .options(selectinload(TestRun.snapshot))
            .where(TestRun.workspace_id == workspace_id)
            .order_by(TestRun.created_at.desc())
        )
        result = await self.db.execute(stmt)
        return list(result.scalars().all())

    async def get_run(self, run_id: int) -> TestRun:
        stmt = select(TestRun).options(selectinload(TestRun.snapshot)).where(TestRun.id == run_id)
        result = await self.db.execute(stmt)
        run = result.scalar_one_or_none()
        if not run:
            raise TestRunNotFoundError
        return run

    async def get_run_snapshot(self, run_id: int) -> TestRunSignalSnapshot:
        stmt = (
            select(TestRunSignalSnapshot)
            .options(selectinload(TestRunSignalSnapshot.entries))
            .where(TestRunSignalSnapshot.test_run_id == run_id)
        )
        result = await self.db.execute(stmt)
        snapshot = result.scalar_one_or_none()
        if not snapshot:
            raise TestRunNotFoundError
        return snapshot

    async def _load_run_for_repeat(self, run_id: int) -> TestRun:
        stmt = (
            select(TestRun)
            .options(
                selectinload(TestRun.allocation).selectinload(Allocation.entries),
                selectinload(TestRun.sequence_links),
                selectinload(TestRun.snapshot).selectinload(TestRunSignalSnapshot.entries),
            )
            .where(TestRun.id == run_id)
        )
        result = await self.db.execute(stmt)
        run = result.scalar_one_or_none()
        if not run:
            raise TestRunNotFoundError
        return run

    async def create_run(self, payload: TestRunCreateSchema) -> TestRun:
        workspace_id = payload.workspace_id
        sequence_ids = self._validate_sequence_ids(payload.sequence_ids)
        await self._ensure_sequence_membership(workspace_id, sequence_ids)
        allocation_payload = payload.allocation or AllocationCreateSchema()
        sequences = await self._load_sequences(sequence_ids)
        allocation_payload, signal_map = await self._validate_allocation(
            workspace_id,
            allocation_payload,
            sequences,
            allow_empty_allocation=payload.allow_empty_allocation,
        )

        async with self.db.begin():
            run = TestRun(
                workspace_id=workspace_id,
                status=TestRunStatus.CREATED,
                allocation=self._build_allocation_model(allocation_payload, signal_map),
                sequence_links=[
                    TestRunSequenceLink(sequence_id=sequence_id) for sequence_id in sequence_ids
                ],
            )
            self.db.add(run)
            await self.db.flush()
            snapshot = self._build_snapshot_model(
                test_run_id=run.id,
                workspace_id=workspace_id,
                allocation_entries=run.allocation.entries,
                signal_map=signal_map,
            )
            self.db.add(snapshot)

        await self.db.refresh(run)
        return run

    async def repeat_run(self, run_id: int) -> TestRun:
        run = await self._load_run_for_repeat(run_id)
        if not run.snapshot:
            raise TestRunNotFoundError
        if not run.allocation:
            raise TestRunAllocationMissingError("Test run allocation is missing")
        clone = TestRun(
            workspace_id=run.workspace_id,
            status=TestRunStatus.CREATED,
            allocation=self._clone_allocation_model(run.allocation),
            sequence_links=[
                TestRunSequenceLink(sequence_id=link.sequence_id)
                for link in run.sequence_links
            ],
        )
        self.db.add(clone)
        await self.db.flush()
        snapshot = self._clone_snapshot_model(run.snapshot, clone.id)
        self.db.add(snapshot)
        await self.db.commit()
        await self.db.refresh(clone)
        return clone

    async def build_execution_context(self, test_run_id: int) -> ExecutionContext:
        """Load a TestRun with allocation, sequences, and optional snapshot."""
        stmt = (
            select(TestRun)
            .options(
                selectinload(TestRun.allocation).selectinload(Allocation.entries),
                selectinload(TestRun.sequences).selectinload(Sequence.steps),
                selectinload(TestRun.sequence_links),
                selectinload(TestRun.snapshot).selectinload(TestRunSignalSnapshot.entries),
            )
            .where(TestRun.id == test_run_id)
        )
        result = await self.db.execute(stmt)
        run = result.scalar_one_or_none()
        if not run:
            raise TestRunNotFoundError

        if not run.allocation:
            raise TestRunAllocationMissingError("Test run allocation is missing")
        if not run.sequences:
            raise TestRunSequencesMissingError("Test run has no sequences attached")

        allocation_entries = tuple(
            self._build_allocation_entry_dto(entry) for entry in run.allocation.entries
        )
        sequence_map = {sequence.id: sequence for sequence in run.sequences}
        ordered_sequences = [
            sequence_map[link.sequence_id]
            for link in run.sequence_links
            if link.sequence_id in sequence_map
        ]
        sequences = tuple(self._build_sequence_dto(sequence) for sequence in ordered_sequences)
        return ExecutionContext(
            test_run_id=run.id,
            status=run.status,
            allocation_entries=allocation_entries,
            sequences=sequences,
        )

    async def mark_running(self, test_run_id: int) -> TestRun:
        run = await self.get_run(test_run_id)
        if run.status == TestRunStatus.RUNNING:
            return run
        if run.status != TestRunStatus.CREATED:
            raise TestRunInvalidStateError(
                f"Cannot start test run in status {run.status}"
            )
        run.status = TestRunStatus.RUNNING
        run.started_at = run.started_at or datetime.now(timezone.utc)
        await self.db.commit()
        await self.db.refresh(run)
        return run

    async def mark_completed(self, test_run_id: int) -> TestRun:
        run = await self.get_run(test_run_id)
        if run.status == TestRunStatus.COMPLETED:
            return run
        if run.status == TestRunStatus.FAILED:
            return run
        run.status = TestRunStatus.COMPLETED
        run.finished_at = datetime.now(timezone.utc)
        await self.db.commit()
        await self.db.refresh(run)
        return run

    async def mark_failed(self, test_run_id: int, reason: str) -> TestRun:
        run = await self.get_run(test_run_id)
        if run.status == TestRunStatus.FAILED:
            return run
        run.status = TestRunStatus.FAILED
        run.finished_at = datetime.now(timezone.utc)
        meta = dict(run.execution_meta or {})
        meta["error"] = self._short_reason(reason)
        run.execution_meta = meta
        await self.db.commit()
        await self.db.refresh(run)
        return run

    async def _ensure_sequence_membership(self, workspace_id: int, sequence_ids: list[int]) -> None:
        stmt: Select[tuple[int]] = select(WorkspaceSequence.sequence_id).where(
            WorkspaceSequence.workspace_id == workspace_id,
            WorkspaceSequence.sequence_id.in_(sequence_ids),
        )
        result = await self.db.execute(stmt)
        located = set(result.scalars().all())
        missing = [sequence_id for sequence_id in sequence_ids if sequence_id not in located]
        if missing:
            raise WorkspaceResourceNotFoundError(
                "Sequences are not attached to this workspace: " + ", ".join(map(str, missing))
            )

    @staticmethod
    def _validate_sequence_ids(sequence_ids: list[int]) -> list[int]:
        seen: set[int] = set()
        ordered: list[int] = []
        for sequence_id in sequence_ids:
            if sequence_id in seen:
                raise DuplicateSequenceSelectionError("sequence_ids must be unique")
            seen.add(sequence_id)
            ordered.append(sequence_id)
        return ordered

    async def _load_sequences(self, sequence_ids: list[int]) -> list[Sequence]:
        stmt = (
            select(Sequence)
            .options(selectinload(Sequence.steps))
            .where(Sequence.id.in_(sequence_ids))
        )
        result = await self.db.execute(stmt)
        sequence_map = {seq.id: seq for seq in result.scalars().all()}
        return [sequence_map[seq_id] for seq_id in sequence_ids if seq_id in sequence_map]

    async def _validate_allocation(
        self,
        workspace_id: int,
        allocation_payload: AllocationCreateSchema,
        sequences: list[Sequence],
        *,
        allow_empty_allocation: bool,
    ) -> tuple[AllocationCreateSchema, dict[int, Signal]]:
        referenced_channel_ids: set[int] = set()
        referenced_signal_keys: set[str] = set()

        for sequence in sequences:
            seq_channel_ids, seq_signal_keys = self._collect_sequence_references(sequence)
            if not seq_channel_ids and not seq_signal_keys:
                raise SequenceNotApplicableError(
                    f"Sequence {sequence.id} has no channel or signal references"
                )
            referenced_channel_ids.update(seq_channel_ids)
            referenced_signal_keys.update(seq_signal_keys)

        allocation_entries = list(allocation_payload.entries)
        if not allocation_entries:
            if not allow_empty_allocation:
                raise AllocationInvalidError("Allocation must include at least one entry")
            if referenced_signal_keys:
                raise AllocationInvalidError(
                    "Allocation is required to resolve signal keys for this test run"
                )
            allocation_entries = [
                AllocationEntryCreateSchema(channel_id=channel_id)
                for channel_id in sorted(referenced_channel_ids)
            ]

        allocation_channel_ids = {entry.channel_id for entry in allocation_entries}
        allocation_signal_ids = {
            entry.signal_id for entry in allocation_entries if entry.signal_id is not None
        }

        signal_map = await self._load_signals_by_ids(workspace_id, allocation_signal_ids)
        available_signal_ids = set(signal_map.keys())
        missing_signal_ids = sorted(allocation_signal_ids - available_signal_ids)
        if missing_signal_ids:
            missing = ", ".join(map(str, missing_signal_ids))
            raise AllocationInvalidError(f"Signals not found: {missing}")
        allocation_signal_keys = {signal.key for signal in signal_map.values()}

        if referenced_channel_ids - allocation_channel_ids:
            missing = ", ".join(map(str, sorted(referenced_channel_ids - allocation_channel_ids)))
            raise ChannelNotAllocatedError(f"Allocation missing channels: {missing}")
        if referenced_signal_keys - allocation_signal_keys:
            missing = ", ".join(sorted(referenced_signal_keys - allocation_signal_keys))
            raise ChannelNotAllocatedError(f"Allocation missing signal keys: {missing}")

        await self._ensure_channels_exist(referenced_channel_ids | allocation_channel_ids)

        return (
            AllocationCreateSchema(
                notes=allocation_payload.notes,
                entries=allocation_entries,
            ),
            signal_map,
        )

    @staticmethod
    def _collect_sequence_references(sequence: Sequence) -> tuple[set[int], set[str]]:
        channel_ids: set[int] = set()
        signal_keys: set[str] = set()
        for step in sequence.steps:
            if step.channel_id:
                channel_ids.add(step.channel_id)
            payload = step.payload or {}
            for cid in payload.get("channel_ids") or []:
                if cid:
                    channel_ids.add(int(cid))
            signal_key = payload.get("signal_key")
            if signal_key:
                signal_keys.add(str(signal_key))
            for key in payload.get("signal_keys") or []:
                if key:
                    signal_keys.add(str(key))
        return channel_ids, signal_keys

    async def _ensure_channels_exist(self, channel_ids: set[int]) -> None:
        if not channel_ids:
            return
        stmt = select(Channel.id).where(Channel.id.in_(channel_ids))
        result = await self.db.execute(stmt)
        existing = set(result.scalars().all())
        missing = sorted(channel_ids - existing)
        if missing:
            raise ChannelNotFoundError(
                "Channels not found: " + ", ".join(map(str, missing))
            )

    async def _load_signals_by_ids(
        self,
        workspace_id: int,
        signal_ids: set[int],
    ) -> dict[int, Signal]:
        if not signal_ids:
            return {}
        stmt: Select[tuple[Signal]] = select(Signal).where(
            Signal.workspace_id == workspace_id,
            Signal.id.in_(signal_ids),
            Signal.deleted_at.is_(None),
        )
        result = await self.db.execute(stmt)
        signals = result.scalars().all()
        return {signal.id: signal for signal in signals}

    @staticmethod
    def _short_reason(reason: str, max_len: int = 200) -> str:
        normalized = reason.strip()
        if len(normalized) <= max_len:
            return normalized
        return normalized[: max_len - 3] + "..."

    @staticmethod
    def _build_allocation_entry_dto(entry: AllocationEntry) -> AllocationEntryDTO:
        return AllocationEntryDTO(
            channel_id=entry.channel_id,
            signal_id=entry.signal_id,
            signal_key=entry.signal_key,
            signal_metadata=copy.deepcopy(entry.signal_metadata)
            if entry.signal_metadata is not None
            else None,
        )

    @staticmethod
    def _build_sequence_dto(sequence: Sequence) -> SequenceDTO:
        steps = tuple(
            SequenceStepDTO(
                id=step.id,
                order_index=step.order_index,
                sequence_step_type=step.sequence_step_type,
                channel_id=step.channel_id,
                payload=copy.deepcopy(step.payload) if step.payload else None,
            )
            for step in sequence.steps
        )
        return SequenceDTO(
            id=sequence.id,
            steps=steps,
        )

    @staticmethod
    def _build_allocation_model(
        payload: AllocationCreateSchema,
        signal_map: dict[int, Signal],
    ) -> Allocation:
        entries: list[AllocationEntry] = []
        for entry in payload.entries:
            signal = signal_map.get(entry.signal_id) if entry.signal_id is not None else None
            entries.append(
                AllocationEntry(
                    channel_id=entry.channel_id,
                    signal_id=entry.signal_id,
                    signal_key=signal.key if signal else None,
                    signal_metadata=copy.deepcopy(entry.signal_metadata)
                    if entry.signal_metadata is not None
                    else None,
                )
            )
        return Allocation(notes=payload.notes, entries=entries)

    @staticmethod
    def _clone_allocation_model(allocation: Allocation) -> Allocation:
        entries = [
            AllocationEntry(
                channel_id=entry.channel_id,
                signal_id=entry.signal_id,
                signal_key=entry.signal_key,
                signal_metadata=copy.deepcopy(entry.signal_metadata)
                if entry.signal_metadata is not None
                else None,
            )
            for entry in allocation.entries
        ]
        return Allocation(notes=allocation.notes, entries=entries)

    @staticmethod
    def _build_snapshot_model(
        *,
        test_run_id: int,
        workspace_id: int,
        allocation_entries: list[AllocationEntry],
        signal_map: dict[int, Signal],
    ) -> TestRunSignalSnapshot:
        entries: list[TestRunSignalSnapshotEntry] = []
        for entry in allocation_entries:
            if entry.signal_id is None:
                continue
            signal = signal_map.get(entry.signal_id)
            if not signal:
                continue
            entries.append(
                TestRunSignalSnapshotEntry(
                    live_signal_id=signal.id,
                    signal_key=signal.key,
                    name=signal.name,
                    io_direction=signal.io_direction,
                    allocation_channel_id=entry.channel_id,
                    allocation_metadata=copy.deepcopy(entry.signal_metadata)
                    if entry.signal_metadata is not None
                    else None,
                    entry_metadata=copy.deepcopy(signal.signal_metadata),
                )
            )
        return TestRunSignalSnapshot(
            test_run_id=test_run_id,
            workspace_id=workspace_id,
            entries=entries,
        )

    @staticmethod
    def _clone_snapshot_model(
        snapshot: TestRunSignalSnapshot,
        test_run_id: int,
    ) -> TestRunSignalSnapshot:
        entries = [
            TestRunSignalSnapshotEntry(
                live_signal_id=entry.live_signal_id,
                signal_key=entry.signal_key,
                name=entry.name,
                io_direction=entry.io_direction,
                allocation_channel_id=entry.allocation_channel_id,
                allocation_metadata=copy.deepcopy(entry.allocation_metadata)
                if entry.allocation_metadata is not None
                else None,
                entry_metadata=copy.deepcopy(entry.entry_metadata),
            )
            for entry in snapshot.entries
        ]
        return TestRunSignalSnapshot(
            test_run_id=test_run_id,
            workspace_id=snapshot.workspace_id,
            entries=entries,
        )
