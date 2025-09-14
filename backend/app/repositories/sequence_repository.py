from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy import select
from sqlalchemy.exc import SQLAlchemyError
from app.models.sequence import Sequence, SequenceStep


async def create_sequence(db: AsyncSession, data: dict, steps: list[dict]) -> Sequence:
    try:
        seq = Sequence(**data)
        db.add(seq)
        await db.flush()  # чтобы id появился

        for idx, step in enumerate(steps):
            st = SequenceStep(sequence_id=seq.id, order_index=idx, **step)
            db.add(st)

        await db.commit()
        await db.refresh(seq)
        return seq
    except SQLAlchemyError as e:
        await db.rollback()
        raise RuntimeError(f"DB error creating sequence: {e}")


async def get_sequence(db: AsyncSession, seq_id: int) -> Sequence | None:
    result = await db.execute(select(Sequence).where(Sequence.id == seq_id))
    return result.scalar_one_or_none()


async def get_all_sequences(db: AsyncSession) -> list[Sequence]:
    result = await db.execute(select(Sequence))
    return list(result.scalars().all())


async def update_sequence(db: AsyncSession, seq_id: int, changes: dict) -> Sequence | None:
    seq = await get_sequence(db, seq_id)
    if not seq:
        return None
    for k, v in changes.items():
        setattr(seq, k, v)
    await db.commit()
    await db.refresh(seq)
    return seq


async def delete_sequence(db: AsyncSession, seq_id: int) -> bool:
    seq = await get_sequence(db, seq_id)
    if not seq:
        return False
    await db.delete(seq)
    await db.commit()
    return True
