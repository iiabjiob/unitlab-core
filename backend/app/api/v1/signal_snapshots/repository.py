from __future__ import annotations

from datetime import datetime, timezone

from sqlalchemy import delete, select
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy.orm import load_only, selectinload

from app.models.channel import Channel
from app.models.signal_snapshot import SignalSnapshot, SignalSnapshotAllocation, SignalSnapshotStatus
from app.models.workspace import Workspace


class SignalSnapshotsRepository:
    def __init__(self, db: AsyncSession):
        self.db = db

    async def ensure_workspace(self, workspace_id: int) -> bool:
        stmt = select(Workspace.id).where(Workspace.id == workspace_id)
        result = await self.db.execute(stmt.limit(1))
        return result.scalar_one_or_none() is not None

    async def list(self, workspace_id: int, *, limit: int = 200, offset: int = 0) -> list[SignalSnapshot]:
        normalized_limit = max(1, min(limit, 1000))
        normalized_offset = max(0, offset)
        stmt = (
            select(SignalSnapshot)
            .options(
                load_only(
                    SignalSnapshot.id,
                    SignalSnapshot.workspace_id,
                    SignalSnapshot.status,
                    SignalSnapshot.source_filename,
                    SignalSnapshot.source_hash,
                    SignalSnapshot.rows_count,
                    SignalSnapshot.schema_version,
                    SignalSnapshot.locked_at,
                    SignalSnapshot.created_at,
                    SignalSnapshot.updated_at,
                )
            )
            .where(SignalSnapshot.workspace_id == workspace_id)
            .order_by(SignalSnapshot.updated_at.desc())
            .limit(normalized_limit)
            .offset(normalized_offset)
        )
        result = await self.db.execute(stmt)
        return list(result.scalars().all())

    async def get(self, snapshot_id: int) -> SignalSnapshot | None:
        stmt = (
            select(SignalSnapshot)
            .options(selectinload(SignalSnapshot.allocation))
            .where(SignalSnapshot.id == snapshot_id)
        )
        result = await self.db.execute(stmt)
        return result.scalar_one_or_none()

    async def create(
        self,
        *,
        workspace_id: int,
        source_filename: str | None,
        source_hash: str,
        rows_count: int,
        schema_version: int,
        data: dict,
        import_meta: dict | None,
    ) -> SignalSnapshot:
        snapshot = SignalSnapshot(
            workspace_id=workspace_id,
            status=SignalSnapshotStatus.DRAFT,
            source_filename=source_filename,
            source_hash=source_hash,
            rows_count=rows_count,
            schema_version=schema_version,
            data=data,
            import_meta=import_meta,
        )
        self.db.add(snapshot)
        await self.db.flush()

        allocation = SignalSnapshotAllocation(
            workspace_id=workspace_id,
            signal_snapshot_id=snapshot.id,
            mapping=[],
        )
        self.db.add(allocation)

        await self.db.commit()
        await self.db.refresh(snapshot)
        return snapshot

    async def delete_for_workspace(self, workspace_id: int) -> int:
        stmt = delete(SignalSnapshot).where(SignalSnapshot.workspace_id == workspace_id)
        result = await self.db.execute(stmt)
        await self.db.commit()
        return int(result.rowcount or 0)

    async def delete(self, snapshot_id: int) -> bool:
        snapshot = await self.get(snapshot_id)
        if not snapshot:
            return False
        await self.db.delete(snapshot)
        await self.db.commit()
        return True

    async def lock(self, snapshot_id: int) -> SignalSnapshot | None:
        snapshot = await self.get(snapshot_id)
        if not snapshot:
            return None
        if snapshot.status == SignalSnapshotStatus.LOCKED:
            return snapshot

        snapshot.status = SignalSnapshotStatus.LOCKED
        snapshot.locked_at = datetime.now(timezone.utc)
        await self.db.commit()
        await self.db.refresh(snapshot)
        return snapshot

    async def get_allocation(self, snapshot_id: int) -> SignalSnapshotAllocation | None:
        stmt = select(SignalSnapshotAllocation).where(
            SignalSnapshotAllocation.signal_snapshot_id == snapshot_id
        )
        result = await self.db.execute(stmt)
        return result.scalar_one_or_none()

    async def update_allocation(self, snapshot_id: int, mapping: list[dict]) -> SignalSnapshotAllocation | None:
        snapshot = await self.get(snapshot_id)
        if not snapshot:
            return None

        allocation = await self.get_allocation(snapshot_id)
        if not allocation:
            allocation = SignalSnapshotAllocation(
                workspace_id=snapshot.workspace_id,
                signal_snapshot_id=snapshot.id,
                mapping=mapping,
            )
            self.db.add(allocation)
        else:
            allocation.mapping = mapping

        await self.db.commit()
        await self.db.refresh(allocation)
        return allocation

    async def channels_exist(self, channel_ids: set[int]) -> bool:
        if not channel_ids:
            return True
        stmt = select(Channel.id).where(Channel.id.in_(channel_ids))
        rows = await self.db.execute(stmt)
        existing = set(rows.scalars().all())
        return existing == channel_ids
