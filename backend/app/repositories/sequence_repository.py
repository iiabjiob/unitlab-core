from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy import select
from sqlalchemy.orm import selectinload
from sqlalchemy.exc import SQLAlchemyError
from app.models.sequence import Sequence
from app.models.sequence_step import SequenceStep


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
                step_data = {**step, "order_index": idx}  # нормализуем порядок
                st = SequenceStep(sequence_id=seq.id, **step_data)
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
            .options(selectinload(Sequence.steps))
            .where(Sequence.id == seq_id)
        )
        return result.scalar_one_or_none()

    async def get_all(self) -> list[Sequence]:
        result = await self.db.execute(
            select(Sequence).options(selectinload(Sequence.steps))
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
        steps: list[dict]
    ) -> Sequence:
        result = await self.db.execute(
            select(Sequence).where(Sequence.name == seq_data["name"])
        )
        existing = result.scalar_one_or_none()
        if existing:
            return existing

        seq = Sequence(**seq_data)
        for idx, step in enumerate(steps):
            seq.steps.append(SequenceStep(order_index=idx, **step))

        self.db.add(seq)
        await self.db.commit()
        await self.db.refresh(seq)
        return seq
