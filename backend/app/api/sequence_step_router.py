from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy import select

from app.infrastructure.db.database import get_db
from app.repositories.sequence_step_repository import SequenceStepRepository
from app.schemas.sequence_step_schema import (
    SequenceStepSchema,
    SequenceStepCreateSchema,
    SequenceStepUpdateSchema,
    SequenceReorderSchema,
)
from app.models.sequence import SequenceStep

router = APIRouter(prefix="/api/sequences/{seq_id}/steps", tags=["SequenceSteps"])


@router.get("", response_model=list[SequenceStepSchema])
async def list_steps(seq_id: int, db: AsyncSession = Depends(get_db)):
    repo = SequenceStepRepository(db)
    return await repo.get_for_sequence(seq_id)


@router.post("", response_model=SequenceStepSchema)
async def create_step(
    seq_id: int,
    data: SequenceStepCreateSchema,
    db: AsyncSession = Depends(get_db),
):
    repo = SequenceStepRepository(db)
    steps = await repo.get_for_sequence(seq_id)
    next_index = len(steps)

    step_data = data.model_dump()
    step_data["order_index"] = next_index

    return await repo.create(sequence_id=seq_id, data=step_data)


@router.patch("/{step_id}", response_model=SequenceStepSchema)
async def update_step(
    seq_id: int,
    step_id: int,
    data: SequenceStepUpdateSchema,
    db: AsyncSession = Depends(get_db),
):
    repo = SequenceStepRepository(db)
    changes = data.model_dump(exclude_unset=True)

    # forbid direct change of order_index here
    changes.pop("order_index", None)

    step = await repo.update(step_id, changes)
    if not step:
        raise HTTPException(404, "Step not found")
    return step


@router.delete("/{step_id}")
async def delete_step(seq_id: int, step_id: int, db: AsyncSession = Depends(get_db)):
    repo = SequenceStepRepository(db)
    ok = await repo.delete(step_id)
    if not ok:
        raise HTTPException(404, "Step not found")

    # normalize order after deletion
    steps = await repo.get_for_sequence(seq_id)
    for idx, s in enumerate(steps):
        s.order_index = idx
    await db.commit()

    return {"detail": "Step deleted and order normalized"}


@router.post("/reorder", response_model=list[SequenceStepSchema])
async def reorder_steps(
    seq_id: int,
    payload: SequenceReorderSchema,
    db: AsyncSession = Depends(get_db),
):
    result = await db.execute(
        select(SequenceStep).where(SequenceStep.sequence_id == seq_id)
    )
    steps = {step.id: step for step in result.scalars().all()}

    for idx, step_id in enumerate(payload.new_order):
        if step_id in steps:
            steps[step_id].order_index = idx

    await db.commit()

    result = await db.execute(
        select(SequenceStep)
        .where(SequenceStep.sequence_id == seq_id)
        .order_by(SequenceStep.order_index)
    )
    return result.scalars().all()


@router.put("", response_model=list[SequenceStepSchema])
async def replace_steps(
    seq_id: int,
    new_steps: list[SequenceStepCreateSchema],
    db: AsyncSession = Depends(get_db),
):
    repo = SequenceStepRepository(db)

    # remove old steps
    old_steps = await repo.get_for_sequence(seq_id)
    for s in old_steps:
        await db.delete(s)
    await db.flush()

    # create new steps with normalized order
    created: list[SequenceStep] = []
    for idx, step in enumerate(new_steps):
        st = SequenceStep(sequence_id=seq_id, order_index=idx, **step.model_dump())
        db.add(st)
        created.append(st)

    await db.commit()
    return created
