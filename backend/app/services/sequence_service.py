from sqlalchemy.ext.asyncio import AsyncSession
from app.repositories.sequence_step_repository import get_steps_for_sequence
from app.models.sequence import SequenceStep


async def reorder_steps(db: AsyncSession, sequence_id: int, new_order: list[int]) -> None:
    """Reorder steps inside a sequence by updating order_index"""
    steps = await get_steps_for_sequence(db, sequence_id)
    step_map = {s.id: s for s in steps}

    for idx, step_id in enumerate(new_order):
        step = step_map.get(step_id)
        if step:
            step.order_index = idx

    await db.commit()
