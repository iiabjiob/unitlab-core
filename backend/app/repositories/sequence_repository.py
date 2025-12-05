from typing import Any, Dict, List

from sqlalchemy import select
from sqlalchemy.exc import SQLAlchemyError
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy.orm import selectinload

from app.models.sequence import Sequence
from app.models.sequence_step import SequenceStep
from app.models.sequence import SequenceStepType
from app.models.channel import Channel


class SequenceRepository:
    """Repository for CRUD operations on Sequence."""

    def __init__(self, db: AsyncSession):
        self.db = db

    async def create(self, data: dict, steps: list[dict]) -> Sequence:
        try:
            seq = Sequence(**data)
            self.db.add(seq)
            await self.db.flush()  # чтобы id появился

            for idx, step in enumerate(steps):
                step_type_value = step.get("sequence_step_type") or step.get("type") or step.get("kind")
                step_type = SequenceStepType(step_type_value) if step_type_value else SequenceStepType.WAIT
                step_data = {
                    "sequence_id": seq.id,
                    "order_index": idx,
                    "sequence_step_type": step_type,
                    "channel_id": step.get("channel_id"),
                    "payload": step.get("payload"),
                }
                st = SequenceStep(**step_data)
                self.db.add(st)

            await self.db.commit()
            await self.db.refresh(seq, attribute_names=["steps"])
            return seq
        except SQLAlchemyError as e:
            await self.db.rollback()
            raise RuntimeError(f"DB error creating sequence: {e}")

    async def get(self, seq_id: int) -> Sequence | None:
        result = await self.db.execute(
            select(Sequence)
            .options(
                selectinload(Sequence.steps)
                .selectinload(SequenceStep.channel)
                .selectinload(Channel.device)
            )
            .where(Sequence.id == seq_id)
        )
        return result.scalar_one_or_none()

    async def list(self) -> list[Sequence]:
        result = await self.db.execute(
            select(Sequence).options(
                selectinload(Sequence.steps)
                .selectinload(SequenceStep.channel)
                .selectinload(Channel.device)
            )
        )
        return list(result.scalars().all())

    async def update(self, seq_id: int, changes: dict) -> Sequence | None:
        seq = await self.get(seq_id)
        if not seq:
            return None
        for k, v in changes.items():
            setattr(seq, k, v)
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

    async def register_if_not_exists(
        self,
        seq_data: dict,
        steps: List[Dict[str, Any]]
    ) -> Sequence:
        result = await self.db.execute(
            select(Sequence).where(Sequence.name == seq_data["name"])
        )
        existing = result.scalar_one_or_none()
        if existing:
            return existing

        seq = Sequence(**seq_data)
        for idx, step in enumerate(steps):
            step_type_value = step.get("sequence_step_type") or step.get("type") or step.get("kind")
            step_type = SequenceStepType(step_type_value) if step_type_value else SequenceStepType.WAIT
            seq.steps.append(
                SequenceStep(
                    order_index=idx,
                    sequence_step_type=step_type,
                    channel_id=step.get("channel_id"),
                    payload=step.get("payload"),
                )
            )

        self.db.add(seq)
        await self.db.commit()
        await self.db.refresh(seq)
        return seq
