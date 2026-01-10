"""Business logic for allocation management."""
from __future__ import annotations

from typing import Any

from sqlalchemy import Select, select
from sqlalchemy.ext.asyncio import AsyncSession

from app.models.signal_snapshot import Allocation, SignalSnapshot
from app.services.signal_snapshot_service import (
    SignalSnapshotNotFoundError,
    SnapshotLockedError,
)


class AllocationService:
    """Coordinates allocation persistence with snapshot lifecycle rules."""

    def __init__(self, db: AsyncSession):
        self.db = db

    async def get_allocation(self, snapshot_id: int) -> Allocation:
        allocation = await self._fetch_allocation(snapshot_id)
        if allocation:
            return allocation

        snapshot = await self._fetch_snapshot(select(SignalSnapshot).where(SignalSnapshot.id == snapshot_id))
        if not snapshot:
            raise SignalSnapshotNotFoundError

        allocation = Allocation(
            workspace_id=snapshot.workspace_id,
            signal_snapshot_id=snapshot.id,
            mapping=[],
        )
        self.db.add(allocation)
        await self.db.commit()
        await self.db.refresh(allocation)
        return allocation

    async def set_allocation(self, snapshot_id: int, mapping: list[dict[str, Any]]) -> Allocation:
        stmt = select(SignalSnapshot).where(SignalSnapshot.id == snapshot_id).with_for_update()
        snapshot = await self._fetch_snapshot(stmt)
        if not snapshot:
            raise SignalSnapshotNotFoundError
        if snapshot.is_locked():
            raise SnapshotLockedError

        allocation = await self._fetch_allocation(snapshot_id, for_update=True)
        if not allocation:
            allocation = Allocation(
                workspace_id=snapshot.workspace_id,
                signal_snapshot_id=snapshot.id,
                mapping=[],
            )
            self.db.add(allocation)
            await self.db.flush()

        allocation.mapping = mapping
        await self.db.commit()
        await self.db.refresh(allocation)
        return allocation

    async def _fetch_snapshot(self, stmt: Select) -> SignalSnapshot | None:
        result = await self.db.execute(stmt)
        return result.scalar_one_or_none()

    async def _fetch_allocation(self, snapshot_id: int, *, for_update: bool = False) -> Allocation | None:
        stmt = select(Allocation).where(Allocation.signal_snapshot_id == snapshot_id)
        if for_update:
            stmt = stmt.with_for_update()
        result = await self.db.execute(stmt)
        return result.scalar_one_or_none()
