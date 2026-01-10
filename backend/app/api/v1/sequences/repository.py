from __future__ import annotations

from typing import Optional

from sqlalchemy import select
from sqlalchemy.exc import SQLAlchemyError
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy.orm import selectinload

from app.models.channel import Channel
from app.models.sequence import Sequence, SequenceStep, SequenceStepType
from app.models.workspace import WorkspaceSequence


class SequenceRepository:
    """CRUD helpers for `Sequence` plus eager-loading helpers used by the v1 API."""

    def __init__(self, db: AsyncSession):
        self.db = db

    async def list(self, workspace_id: int) -> list[Sequence]:
        result = await self.db.execute(self._with_steps(workspace_id))
        return list(result.scalars().all())

    async def get(self, workspace_id: int, seq_id: int) -> Optional[Sequence]:
        result = await self.db.execute(
            self._with_steps(workspace_id).where(Sequence.id == seq_id)
        )
        return result.scalar_one_or_none()

    async def create(self, workspace_id: int, data: dict, steps: list[dict]) -> Sequence:
        try:
            seq = Sequence(**data)
            self.db.add(seq)
            await self.db.flush()
            self.db.add(WorkspaceSequence(workspace_id=workspace_id, sequence_id=seq.id))

            for idx, raw in enumerate(steps):
                step_type_value = raw.get("sequence_step_type") or raw.get("type") or raw.get("kind")
                step_type = SequenceStepType(step_type_value) if step_type_value else SequenceStepType.WAIT
                payload = {
                    "sequence_id": seq.id,
                    "order_index": idx,
                    "sequence_step_type": step_type,
                    "channel_id": raw.get("channel_id"),
                    "payload": raw.get("payload"),
                }
                self.db.add(SequenceStep(**payload))

            await self.db.commit()
            await self.db.refresh(seq, attribute_names=["steps"])
            return seq
        except SQLAlchemyError as exc:  # pragma: no cover - defensive rollback
            await self.db.rollback()
            raise RuntimeError(f"DB error creating sequence: {exc}") from exc

    async def update(self, workspace_id: int, seq_id: int, changes: dict) -> Optional[Sequence]:
        seq = await self.get(workspace_id, seq_id)
        if not seq:
            return None
        for key, value in changes.items():
            setattr(seq, key, value)
        await self.db.commit()
        await self.db.refresh(seq, attribute_names=["steps"])
        return seq

    async def delete(self, workspace_id: int, seq_id: int) -> bool:
        seq = await self.get(workspace_id, seq_id)
        if not seq:
            return False
        await self.db.delete(seq)
        await self.db.commit()
        return True

    async def ensure(self, workspace_id: int, seq_id: int) -> bool:
        stmt = (
            select(Sequence.id)
            .join(WorkspaceSequence, WorkspaceSequence.sequence_id == Sequence.id)
            .where(
                Sequence.id == seq_id,
                WorkspaceSequence.workspace_id == workspace_id,
            )
        )
        result = await self.db.execute(stmt)
        return result.scalar_one_or_none() is not None

    def _with_steps(self, workspace_id: int | None = None):
        stmt = select(Sequence).options(
            selectinload(Sequence.steps)
            .selectinload(SequenceStep.channel)
            .selectinload(Channel.device),
            selectinload(Sequence.workspaces),
        )
        if workspace_id is not None:
            stmt = stmt.join(WorkspaceSequence, WorkspaceSequence.sequence_id == Sequence.id).where(
                WorkspaceSequence.workspace_id == workspace_id
            )
        return stmt
