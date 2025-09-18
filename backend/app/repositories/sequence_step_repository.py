from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy import select
from sqlalchemy.exc import SQLAlchemyError
from app.models.sequence import SequenceStep


async def create_step(db: AsyncSession, sequence_id: int, data: dict) -> SequenceStep:
    try:
        step = SequenceStep(sequence_id=sequence_id, **data)
        db.add(step)
        await db.commit()
        await db.refresh(step)
        return step
    except SQLAlchemyError as e:
        await db.rollback()
        raise RuntimeError(f"DB error creating step: {e}")


async def get_step(db: AsyncSession, step_id: int) -> SequenceStep | None:
    result = await db.execute(select(SequenceStep).where(SequenceStep.id == step_id))
    return result.scalar_one_or_none()


async def get_steps_for_sequence(db: AsyncSession, sequence_id: int) -> list[SequenceStep]:
    result = await db.execute(
        select(SequenceStep)
        .where(SequenceStep.sequence_id == sequence_id)
        .order_by(SequenceStep.order_index)
    )
    return result.scalars().all()


async def update_step(db: AsyncSession, step_id: int, changes: dict) -> SequenceStep | None:
    step = await get_step(db, step_id)
    if not step:
        return None
    for k, v in changes.items():
        setattr(step, k, v)
    await db.commit()
    await db.refresh(step)
    return step


async def delete_step(db: AsyncSession, step_id: int) -> bool:
    step = await get_step(db, step_id)
    if not step:
        return False
    await db.delete(step)
    await db.commit()
    return True
