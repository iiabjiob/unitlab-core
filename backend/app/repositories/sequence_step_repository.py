from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy import select
from sqlalchemy.exc import SQLAlchemyError
from app.models.sequence_step import SequenceStep


class SequenceStepRepository:
    """Repository for CRUD operations on SequenceStep."""

    def __init__(self, db: AsyncSession):
        self.db = db

    async def create(self, sequence_id: int, data: dict) -> SequenceStep:
        try:
            step = SequenceStep(sequence_id=sequence_id, **data)
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
