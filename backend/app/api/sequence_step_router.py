from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy.ext.asyncio import AsyncSession

from app.infrastructure.db.database import get_db
from app.repositories.sequence_step_repository import (
    create_step as create_step_repo,
    get_steps_for_sequence,
    update_step as update_step_repo,
    delete_step as delete_step_repo,
)
from app.services.sequence_service import reorder_steps as reorder_steps_service
from app.schemas.sequence_step_schema import (
    SequenceStepSchema,
    SequenceStepCreateSchema,
    SequenceStepUpdateSchema,
)
from app.models.sequence import SequenceStep

router = APIRouter(prefix="/api/sequences/{seq_id}/steps", tags=["SequenceSteps"])


@router.get("", response_model=list[SequenceStepSchema])
async def list_steps(seq_id: int, db: AsyncSession = Depends(get_db)):
    return await get_steps_for_sequence(db, seq_id)


@router.post("", response_model=SequenceStepSchema)
async def create_step(seq_id: int, data: SequenceStepCreateSchema, db: AsyncSession = Depends(get_db)):
    # find current max order_index
    steps = await get_steps_for_sequence(db, seq_id)
    next_index = len(steps)
    step_data = data.model_dump()
    step_data["order_index"] = next_index
    return await create_step_repo(db, sequence_id=seq_id, data=step_data)


@router.patch("/{step_id}", response_model=SequenceStepSchema)
async def update_step(seq_id: int, step_id: int, data: SequenceStepUpdateSchema, db: AsyncSession = Depends(get_db)):
    # do not allow changing order_index here → only via reorder/batch update
    changes = data.model_dump(exclude_unset=True)
    if "order_index" in changes:
        changes.pop("order_index")
    step = await update_step_repo(db, step_id, changes)
    if not step:
        raise HTTPException(404, "Step not found")
    return step


@router.delete("/{step_id}")
async def delete_step(seq_id: int, step_id: int, db: AsyncSession = Depends(get_db)):
    ok = await delete_step_repo(db, step_id)
    if not ok:
        raise HTTPException(404, "Step not found")

    # normalize order after deletion
    steps = await get_steps_for_sequence(db, seq_id)
    for idx, s in enumerate(steps):
        s.order_index = idx
    await db.commit()

    return {"detail": "Step deleted and order normalized"}


@router.post("/reorder")
async def reorder_steps(seq_id: int, new_order: list[int], db: AsyncSession = Depends(get_db)):
    await reorder_steps_service(db, seq_id, new_order)
    return {"detail": "Steps reordered"}


@router.put("", response_model=list[SequenceStepSchema])
async def replace_steps(seq_id: int, new_steps: list[SequenceStepCreateSchema], db: AsyncSession = Depends(get_db)):
    # remove old steps
    old_steps = await get_steps_for_sequence(db, seq_id)
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
