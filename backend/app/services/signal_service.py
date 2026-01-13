"""CRUD service for live workspace signals."""
from __future__ import annotations

from datetime import datetime, timezone

from sqlalchemy import Select, select
from sqlalchemy.ext.asyncio import AsyncSession

from app.models.signal import Signal
from app.schemas.signal_schema import SignalCreateSchema, SignalUpdateSchema


class SignalNotFoundError(Exception):
    """Raised when a signal record cannot be located."""


class SignalKeyConflictError(Exception):
    """Raised when attempting to reuse an existing signal key inside a workspace."""


class SignalService:
    def __init__(self, db: AsyncSession):
        self.db = db

    async def list_signals(self, workspace_id: int, *, include_deleted: bool = False) -> list[Signal]:
        stmt = (
            select(Signal)
            .where(Signal.workspace_id == workspace_id)
            .order_by(Signal.name.asc(), Signal.id.asc())
        )
        if not include_deleted:
            stmt = stmt.where(Signal.deleted_at.is_(None))
        result = await self.db.execute(stmt)
        return list(result.scalars().all())

    async def get_signal(self, signal_id: int) -> Signal:
        signal = await self._get_signal(signal_id)
        if not signal or signal.deleted_at is not None:
            raise SignalNotFoundError
        return signal

    async def create_signal(self, workspace_id: int, payload: SignalCreateSchema) -> Signal:
        await self._ensure_key_available(workspace_id, payload.key)
        signal = Signal(
            workspace_id=workspace_id,
            key=payload.key,
            name=payload.name,
            io_direction=payload.io_direction,
            category=payload.category,
            signal_metadata=dict(payload.metadata or {}),
            is_active=payload.is_active,
        )
        self.db.add(signal)
        await self.db.commit()
        await self.db.refresh(signal)
        return signal

    async def update_signal(self, signal_id: int, payload: SignalUpdateSchema) -> Signal:
        signal = await self._get_signal(signal_id)
        if not signal or signal.deleted_at is not None:
            raise SignalNotFoundError
        updates = payload.model_dump(exclude_unset=True)
        if "name" in updates:
            signal.name = updates["name"]
        if "io_direction" in updates:
            signal.io_direction = updates["io_direction"]
        if "category" in updates:
            signal.category = updates["category"]
        if "metadata" in updates:
            signal.signal_metadata = dict(updates["metadata"] or {})
        if "is_active" in updates:
            signal.is_active = updates["is_active"]
        await self.db.commit()
        await self.db.refresh(signal)
        return signal

    async def delete_signal(self, signal_id: int) -> None:
        signal = await self._get_signal(signal_id)
        if not signal or signal.deleted_at is not None:
            raise SignalNotFoundError
        signal.deleted_at = datetime.now(timezone.utc)
        signal.is_active = False
        await self.db.commit()

    async def _get_signal(self, signal_id: int) -> Signal | None:
        stmt: Select[tuple[Signal]] = select(Signal).where(Signal.id == signal_id)
        result = await self.db.execute(stmt)
        return result.scalar_one_or_none()

    async def _ensure_key_available(self, workspace_id: int, key: str) -> None:
        stmt = select(Signal.id).where(
            Signal.workspace_id == workspace_id,
            Signal.key == key,
        )
        result = await self.db.execute(stmt)
        if result.scalar_one_or_none() is not None:
            raise SignalKeyConflictError
