from sqlalchemy import select
from sqlalchemy.exc import SQLAlchemyError
from sqlalchemy.ext.asyncio import AsyncSession

from app.models.sequence import SequenceStepType
from app.models.sequence_step import SequenceStep


class SequenceStepRepository:
    """Repository for CRUD operations on SequenceStep."""

    def __init__(self, db: AsyncSession):
        self.db = db

    async def create(self, sequence_id: int, data: dict) -> SequenceStep:
        try:
            result = await self.db.execute(
                select(SequenceStep.order_index)
                .where(SequenceStep.sequence_id == sequence_id)
                .order_by(SequenceStep.order_index.desc())
                .limit(1)
            )
            last_index = result.scalar_one_or_none()
            if last_index is None:
                last_index = -1

            step_type_value = data.get("sequence_step_type") or data.get("type") or data.get("kind")
            step_type = SequenceStepType(step_type_value) if step_type_value else SequenceStepType.WAIT

            step = SequenceStep(
                sequence_id=sequence_id,
                order_index=last_index + 1,
                sequence_step_type=step_type,
                channel_id=data.get("channel_id"),
                payload=data.get("payload"),
            )
            self.db.add(step)
            await self.db.commit()
            await self.db.refresh(step)
            return step
        except SQLAlchemyError as e:
            await self.db.rollback()
            raise RuntimeError(f"DB error creating step: {e}")

    async def get(self, step_id: int) -> SequenceStep | None:
        result = await self.db.execute(
            select(SequenceStep).where(SequenceStep.id == step_id)
        )
        return result.scalar_one_or_none()

    async def get_for_sequence(self, sequence_id: int) -> list[SequenceStep]:
        result = await self.db.execute(
            select(SequenceStep)
            .where(SequenceStep.sequence_id == sequence_id)
            .order_by(SequenceStep.order_index)
        )
        return result.scalars().all()

    async def update(self, step_id: int, changes: dict) -> SequenceStep | None:
        step = await self.get(step_id)
        if not step:
            return None
        for k, v in changes.items():
            if k in {"sequence_step_type", "type", "kind"} and v is not None:
                step.sequence_step_type = SequenceStepType(v)
            elif k == "order_index" and v is not None:
                step.order_index = int(v)
            elif hasattr(step, k):
                setattr(step, k, v)
        await self.db.commit()
        await self.db.refresh(step)
        return step

    async def delete(self, step_id: int) -> bool:
        step = await self.get(step_id)
        if not step:
            return False
        await self.db.delete(step)
        await self.db.commit()
        return True

    async def reorder(self, sequence_id: int, new_order: list[int]) -> list[SequenceStep]:
        steps = await self.get_for_sequence(sequence_id)
        mapping = {step.id: step for step in steps}
        for idx, step_id in enumerate(new_order):
            if step_id in mapping:
                mapping[step_id].order_index = idx
        await self.db.commit()
        return await self.get_for_sequence(sequence_id)

    async def replace(self, sequence_id: int, new_steps: list[dict]) -> list[SequenceStep]:
        existing = await self.get_for_sequence(sequence_id)
        for step in existing:
            await self.db.delete(step)
        await self.db.flush()

        created: list[SequenceStep] = []
        for idx, payload in enumerate(new_steps):
            step_type_value = payload.get("sequence_step_type") or payload.get("type") or payload.get("kind")
            step_type = SequenceStepType(step_type_value) if step_type_value else SequenceStepType.WAIT
            step = SequenceStep(
                sequence_id=sequence_id,
                order_index=idx,
                sequence_step_type=step_type,
                channel_id=payload.get("channel_id"),
                payload=payload.get("payload"),
            )
            self.db.add(step)
            created.append(step)

        await self.db.commit()
        return await self.get_for_sequence(sequence_id)
