from __future__ import annotations

from datetime import datetime, timezone
from typing import Iterable, Sequence

from sqlalchemy import delete, select, update
from sqlalchemy.ext.asyncio import AsyncSession

from app.models.signal import Signal, SignalIODirection
from app.models.signal_sheet import SignalAllocation
from app.models.workspace import Workspace
from app.services.signal_sheet_import_service import ImportedSignalProjection


class SignalsRepository:
    def __init__(self, db: AsyncSession):
        self.db = db

    async def ensure_workspace(self, workspace_id: int) -> bool:
        stmt = select(Workspace.id).where(Workspace.id == workspace_id)
        result = await self.db.execute(stmt.limit(1))
        return result.scalar_one_or_none() is not None

    async def list(self, workspace_id: int) -> list[Signal]:
        stmt = (
            select(Signal)
            .where(Signal.workspace_id == workspace_id, Signal.deleted_at.is_(None))
            .order_by(Signal.created_at.desc())
        )
        result = await self.db.execute(stmt)
        return list(result.scalars().all())

    async def get(self, signal_id: int) -> Signal | None:
        stmt = select(Signal).where(Signal.id == signal_id)
        result = await self.db.execute(stmt)
        return result.scalar_one_or_none()

    async def create(self, workspace_id: int, payload: dict) -> Signal:
        direction = self._parse_direction(payload["io_direction"])
        signal = Signal(
            workspace_id=workspace_id,
            key=str(payload["key"]).strip(),
            name=str(payload["name"]).strip(),
            io_direction=direction,
            category=payload.get("category"),
            signal_metadata=payload.get("metadata") or {},
            tested_at=self._extract_tested_at_from_metadata(payload.get("metadata") or {}),
            is_active=bool(payload.get("is_active", True)),
        )
        self.db.add(signal)
        await self.db.commit()
        await self.db.refresh(signal)
        return signal

    async def update(self, signal_id: int, payload: dict) -> Signal | None:
        signal = await self.get(signal_id)
        if not signal:
            return None

        if "name" in payload and payload["name"] is not None:
            signal.name = str(payload["name"]).strip()
        if "io_direction" in payload and payload["io_direction"] is not None:
            signal.io_direction = self._parse_direction(payload["io_direction"])
        if "category" in payload:
            signal.category = payload["category"]
        if "metadata" in payload:
            signal.signal_metadata = payload["metadata"] or {}
            signal.tested_at = self._extract_tested_at_from_metadata(signal.signal_metadata)
        if "is_active" in payload and payload["is_active"] is not None:
            signal.is_active = bool(payload["is_active"])

        await self.db.commit()
        await self.db.refresh(signal)
        return signal

    async def delete(self, signal_id: int) -> bool:
        signal = await self.get(signal_id)
        if not signal:
            return False

        await self.db.execute(
            delete(SignalAllocation).where(
                SignalAllocation.workspace_id == signal.workspace_id,
                SignalAllocation.signal_id == signal.id,
            )
        )

        signal.deleted_at = datetime.now(timezone.utc)
        signal.is_active = False
        await self.db.commit()
        return True

    async def delete_many(self, workspace_id: int, signal_ids: Sequence[int]) -> int:
        normalized_ids = {
            int(signal_id)
            for signal_id in signal_ids
            if isinstance(signal_id, int) and signal_id > 0
        }
        if not normalized_ids:
            return 0

        await self.db.execute(
            delete(SignalAllocation).where(
                SignalAllocation.workspace_id == workspace_id,
                SignalAllocation.signal_id.in_(normalized_ids),
            )
        )

        deleted_at = datetime.now(timezone.utc)
        stmt = (
            update(Signal)
            .where(
                Signal.workspace_id == workspace_id,
                Signal.deleted_at.is_(None),
                Signal.id.in_(normalized_ids),
            )
            .values(
                deleted_at=deleted_at,
                is_active=False,
            )
        )
        result = await self.db.execute(stmt)
        deleted_count = int(result.rowcount or 0)
        await self.db.commit()
        return deleted_count

    async def upsert_imported(
        self,
        workspace_id: int,
        projections: Iterable[ImportedSignalProjection],
    ) -> None:
        keys = [item.key for item in projections]
        existing_by_key: dict[str, Signal] = {}

        if keys:
            stmt = select(Signal).where(
                Signal.workspace_id == workspace_id,
                Signal.key.in_(keys),
            )
            rows = await self.db.execute(stmt)
            existing_by_key = {signal.key: signal for signal in rows.scalars().all()}

        for item in projections:
            current = existing_by_key.get(item.key)
            if current is None:
                signal = Signal(
                    workspace_id=workspace_id,
                    key=item.key,
                    name=item.name,
                    io_direction=self._parse_direction(item.io_direction),
                    category=item.category,
                    signal_metadata=item.signal_metadata,
                    tested_at=None,
                    is_active=True,
                    deleted_at=None,
                )
                self.db.add(signal)
                continue

            current.name = item.name
            current.io_direction = self._parse_direction(item.io_direction)
            current.category = item.category
            current.signal_metadata = item.signal_metadata
            current.tested_at = None
            current.is_active = True
            current.deleted_at = None

        await self.db.flush()

    async def replace_from_import(
        self,
        workspace_id: int,
        projections: Iterable[ImportedSignalProjection],
    ) -> None:
        payload = list(projections)
        await self.upsert_imported(workspace_id, payload)

        imported_keys = {item.key for item in payload}
        if not imported_keys:
            stmt = select(Signal).where(
                Signal.workspace_id == workspace_id,
                Signal.deleted_at.is_(None),
            )
            rows = await self.db.execute(stmt)
            for signal in rows.scalars().all():
                signal.is_active = False
                signal.deleted_at = datetime.now(timezone.utc)
            await self.db.flush()
            return

        stale_stmt = select(Signal).where(
            Signal.workspace_id == workspace_id,
            Signal.deleted_at.is_(None),
            Signal.key.not_in(imported_keys),
        )
        stale_rows = await self.db.execute(stale_stmt)
        for stale in stale_rows.scalars().all():
            stale.is_active = False
            stale.deleted_at = datetime.now(timezone.utc)

        await self.db.flush()

    @staticmethod
    def _parse_direction(value: str) -> SignalIODirection:
        if isinstance(value, SignalIODirection):
            return value
        return SignalIODirection(str(value).strip().upper())

    @staticmethod
    def _extract_tested_at_from_metadata(metadata: dict | None) -> datetime | None:
        if not isinstance(metadata, dict):
            return None
        raw_value = metadata.get("tested_at") or metadata.get("testedAt")
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
