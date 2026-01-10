"""Business logic for creating and repeating test runs."""
from __future__ import annotations

import copy

from sqlalchemy import Select, select
from sqlalchemy.ext.asyncio import AsyncSession

from app.models.signal_snapshot import Allocation, SignalSnapshot
from app.models.test_run import TestRun, TestRunStatus
from app.models.workspace import WorkspaceSequence
from app.services.signal_snapshot_service import (
    SignalSnapshotNotFoundError,
    SignalSnapshotService,
)


class WorkspaceResourceNotFoundError(Exception):
    """Raised when a workspace-scoped asset is missing."""


class AllocationMissingError(Exception):
    """Raised when a snapshot has no allocation mapping."""


class TestRunNotFoundError(Exception):
    """Raised when a TestRun record cannot be found."""


class TestRunService:
    def __init__(self, db: AsyncSession):
        self.db = db

    async def list_runs(self, workspace_id: int) -> list[TestRun]:
        stmt = (
            select(TestRun)
            .where(TestRun.workspace_id == workspace_id)
            .order_by(TestRun.created_at.desc())
        )
        result = await self.db.execute(stmt)
        return list(result.scalars().all())

    async def get_run(self, run_id: int) -> TestRun:
        stmt = select(TestRun).where(TestRun.id == run_id)
        result = await self.db.execute(stmt)
        run = result.scalar_one_or_none()
        if not run:
            raise TestRunNotFoundError
        return run

    async def create_run(self, workspace_id: int, sequence_id: int, snapshot_id: int) -> TestRun:
        await self._ensure_sequence_membership(workspace_id, sequence_id)

        async with self.db.begin():
            snapshot = await self._get_snapshot(snapshot_id, workspace_id, for_update=True)
            if not snapshot:
                raise SignalSnapshotNotFoundError

            allocation = await self._get_allocation(snapshot_id, workspace_id, for_update=True)
            if not allocation:
                raise AllocationMissingError("Allocation mapping is required before running tests")

            SignalSnapshotService.lock_snapshot(snapshot)

            run = TestRun(
                workspace_id=workspace_id,
                sequence_id=sequence_id,
                signal_snapshot_id=snapshot.id,
                allocation_snapshot=self._clone_json(allocation.mapping),
                status=TestRunStatus.CREATED,
            )
            self.db.add(run)

        await self.db.refresh(run)
        return run

    async def repeat_run(self, run_id: int) -> TestRun:
        run = await self.get_run(run_id)
        clone = TestRun(
            workspace_id=run.workspace_id,
            sequence_id=run.sequence_id,
            signal_snapshot_id=run.signal_snapshot_id,
            allocation_snapshot=self._clone_json(run.allocation_snapshot),
            status=TestRunStatus.CREATED,
        )
        self.db.add(clone)
        await self.db.commit()
        await self.db.refresh(clone)
        return clone

    async def _ensure_sequence_membership(self, workspace_id: int, sequence_id: int) -> None:
        stmt = select(WorkspaceSequence.id).where(
            WorkspaceSequence.workspace_id == workspace_id,
            WorkspaceSequence.sequence_id == sequence_id,
        )
        result = await self.db.execute(stmt)
        if result.scalar_one_or_none() is None:
            raise WorkspaceResourceNotFoundError("Sequence is not attached to this workspace")

    async def _get_snapshot(
        self,
        snapshot_id: int,
        workspace_id: int,
        *,
        for_update: bool = False,
    ) -> SignalSnapshot | None:
        stmt = select(SignalSnapshot).where(
            SignalSnapshot.id == snapshot_id,
            SignalSnapshot.workspace_id == workspace_id,
        )
        if for_update:
            stmt = stmt.with_for_update()
        result = await self.db.execute(stmt)
        return result.scalar_one_or_none()

    async def _get_allocation(
        self,
        snapshot_id: int,
        workspace_id: int,
        *,
        for_update: bool = False,
    ) -> Allocation | None:
        stmt = select(Allocation).where(
            Allocation.signal_snapshot_id == snapshot_id,
            Allocation.workspace_id == workspace_id,
        )
        if for_update:
            stmt = stmt.with_for_update()
        result = await self.db.execute(stmt)
        return result.scalar_one_or_none()

    @staticmethod
    def _clone_json(payload: list[dict[str, object]] | None) -> list[dict[str, object]]:
        if payload is None:
            return []
        return copy.deepcopy(payload)
