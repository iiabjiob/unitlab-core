"""Signal snapshot import and lifecycle utilities."""
from __future__ import annotations

import csv
import hashlib
import io
from datetime import datetime, timezone
from typing import Any

from sqlalchemy import Select, select
from sqlalchemy.ext.asyncio import AsyncSession

from app.models.signal_snapshot import SignalSnapshot, SignalSnapshotStatus


class InvalidCSVError(Exception):
    """Raised when the uploaded CSV payload cannot be parsed."""


class SignalSnapshotNotFoundError(Exception):
    """Raised when a snapshot record cannot be located."""


class SnapshotLockedError(Exception):
    """Raised when a mutating action targets a locked snapshot."""


class SignalSnapshotService:
    """Coordinates persistence and validation for signal snapshots."""

    def __init__(self, db: AsyncSession):
        self.db = db

    async def import_csv(self, workspace_id: int, payload: bytes, filename: str | None) -> SignalSnapshot:
        rows = self._parse_csv(payload)
        snapshot = SignalSnapshot(
            workspace_id=workspace_id,
            status=SignalSnapshotStatus.DRAFT,
            source_filename=filename,
            source_hash=self._hash_payload(payload),
            rows_count=len(rows),
            schema_version=1,
            data=rows,
        )
        self.db.add(snapshot)
        await self.db.commit()
        await self.db.refresh(snapshot)
        return snapshot

    async def list_snapshots(self, workspace_id: int) -> list[SignalSnapshot]:
        stmt = (
            select(SignalSnapshot)
            .where(SignalSnapshot.workspace_id == workspace_id)
            .order_by(SignalSnapshot.created_at.desc())
        )
        result = await self.db.execute(stmt)
        return list(result.scalars().all())

    async def get_snapshot(self, snapshot_id: int) -> SignalSnapshot:
        snapshot = await self._get_snapshot(snapshot_id)
        if not snapshot:
            raise SignalSnapshotNotFoundError
        return snapshot

    async def delete_snapshot(self, snapshot_id: int) -> None:
        snapshot = await self._get_snapshot(snapshot_id, for_update=True)
        if not snapshot:
            raise SignalSnapshotNotFoundError
        if snapshot.is_locked():
            raise SnapshotLockedError
        await self.db.delete(snapshot)
        await self.db.commit()

    @staticmethod
    def lock_snapshot(snapshot: SignalSnapshot) -> None:
        """Mark the snapshot as immutable for future operations."""
        if snapshot.status == SignalSnapshotStatus.LOCKED:
            return
        snapshot.status = SignalSnapshotStatus.LOCKED
        snapshot.locked_at = datetime.now(timezone.utc)

    async def _get_snapshot(
        self,
        snapshot_id: int,
        *,
        for_update: bool = False,
    ) -> SignalSnapshot | None:
        stmt: Select[tuple[SignalSnapshot]] = select(SignalSnapshot).where(SignalSnapshot.id == snapshot_id)
        if for_update:
            stmt = stmt.with_for_update()
        result = await self.db.execute(stmt)
        return result.scalar_one_or_none()

    @staticmethod
    def _hash_payload(payload: bytes) -> str:
        return hashlib.sha256(payload).hexdigest()

    @staticmethod
    def _parse_csv(payload: bytes) -> list[dict[str, Any]]:
        try:
            buffer = io.StringIO(payload.decode("utf-8-sig"))
        except UnicodeDecodeError as exc:
            raise InvalidCSVError("CSV must be UTF-8 encoded") from exc

        reader = csv.DictReader(buffer)
        if not reader.fieldnames:
            raise InvalidCSVError("CSV header row is required")

        rows: list[dict[str, Any]] = []
        for row in reader:
            rows.append({key: (value if value != "" else None) for key, value in row.items()})

        if not rows:
            raise InvalidCSVError("CSV contains no data rows")

        return rows
