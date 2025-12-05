from __future__ import annotations

from typing import Any, Dict, List, Optional

from sqlalchemy import select
from sqlalchemy.exc import SQLAlchemyError
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy.orm import selectinload

from app.models.channel import Channel
from app.models.sequence import Sequence, SequenceStep, SequenceStepType


class SequenceRepository:
    """CRUD helpers for `Sequence` plus eager-loading helpers used by the v1 API."""

    def __init__(self, db: AsyncSession):
        self.db = db

    async def list(self) -> list[Sequence]:
        result = await self.db.execute(self._with_steps())
        return list(result.scalars().all())

    async def get(self, seq_id: int) -> Optional[Sequence]:
        result = await self.db.execute(self._with_steps().where(Sequence.id == seq_id))
        return result.scalar_one_or_none()

    async def create(self, data: dict, steps: list[dict]) -> Sequence:
        try:
            seq = Sequence(**data)
            self.db.add(seq)
            await self.db.flush()

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

    async def update(self, seq_id: int, changes: dict) -> Optional[Sequence]:
        seq = await self.get(seq_id)
        if not seq:
            return None
        for key, value in changes.items():
            setattr(seq, key, value)
        await self.db.commit()
        await self.db.refresh(seq, attribute_names=["steps"])
        return seq

    async def delete(self, seq_id: int) -> bool:
        seq = await self.get(seq_id)
        if not seq:
            return False
        await self.db.delete(seq)
        await self.db.commit()
        return True

    async def register_if_not_exists(self, seq_data: dict, steps: List[Dict[str, Any]]) -> Sequence:
        result = await self.db.execute(select(Sequence).where(Sequence.name == seq_data["name"]))
        existing = result.scalar_one_or_none()
        if existing:
            return existing

        seq = Sequence(**seq_data)
        for idx, raw in enumerate(steps):
            step_type_value = raw.get("sequence_step_type") or raw.get("type") or raw.get("kind")
            step_type = SequenceStepType(step_type_value) if step_type_value else SequenceStepType.WAIT
            seq.steps.append(
                SequenceStep(
                    order_index=idx,
                    sequence_step_type=step_type,
                    channel_id=raw.get("channel_id"),
                    payload=raw.get("payload"),
                )
            )

        self.db.add(seq)
        await self.db.commit()
        await self.db.refresh(seq)
        return seq

    def _with_steps(self):
        return select(Sequence).options(
            selectinload(Sequence.steps)
            .selectinload(SequenceStep.channel)
            .selectinload(Channel.device)
        )
