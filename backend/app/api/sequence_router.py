# app/api/sequence_router.py
import json, re
from fastapi import APIRouter, Depends, HTTPException, File, UploadFile
from sqlalchemy.ext.asyncio import AsyncSession
from fastapi.responses import JSONResponse
from pydantic import ValidationError
from sqlalchemy import select

from app.infrastructure.db.database import get_db
from app.repositories.sequence_repository import SequenceRepository
from app.models.device import Device
from app.models.channel import Channel
from app.schemas.sequence_schema import (
    SequenceSchema, SequenceCreateSchema, SequenceUpdateSchema,
    SequenceExportSchema
)

router = APIRouter(prefix="/api/sequences", tags=["Sequences"])


@router.get("", response_model=list[SequenceSchema])
async def list_sequences(db: AsyncSession = Depends(get_db)):
    repo = SequenceRepository(db)
    return await repo.list()


@router.get("/{seq_id}", response_model=SequenceSchema)
async def get_one(seq_id: int, db: AsyncSession = Depends(get_db)):
    repo = SequenceRepository(db)
    seq = await repo.get(seq_id)
    if not seq:
        raise HTTPException(404, "Sequence not found")
    return seq


@router.post("", response_model=SequenceSchema)
async def create(data: SequenceCreateSchema, db: AsyncSession = Depends(get_db)):
    repo = SequenceRepository(db)
    steps = [s.model_dump() for s in data.steps]
    return await repo.create(data.model_dump(exclude={"steps"}), steps)


@router.patch("/{seq_id}", response_model=SequenceSchema)
async def update(seq_id: int, data: SequenceUpdateSchema, db: AsyncSession = Depends(get_db)):
    repo = SequenceRepository(db)
    seq = await repo.update(seq_id, data.model_dump(exclude_unset=True))
    if not seq:
        raise HTTPException(404, "Sequence not found")
    return seq


@router.delete("/{seq_id}")
async def delete(seq_id: int, db: AsyncSession = Depends(get_db)):
    repo = SequenceRepository(db)
    ok = await repo.delete(seq_id)
    if not ok:
        raise HTTPException(404, "Sequence not found")
    return {"detail": "Sequence deleted"}


@router.get("/{seq_id}/export-file")
async def export_sequence_file(seq_id: int, db: AsyncSession = Depends(get_db)):
    repo = SequenceRepository(db)
    seq = await repo.get(seq_id)
    if not seq:
        raise HTTPException(404, "Sequence not found")

    payload = {
        "name": seq.name,
        "description": seq.description,
        "steps": [],
    }

    for step in seq.steps:
        if step.channel:  # есть привязка к каналу
            payload["steps"].append({
                "order_index": step.order_index,
                "kind": step.kind,
                "unit_id": step.channel.device.unit_id,
                "channel_index": step.channel.index,
                "payload": step.payload,
            })
        else:  # шаги типа WAIT
            payload["steps"].append({
                "order_index": step.order_index,
                "kind": step.kind,
                "unit_id": None,
                "channel_index": None,
                "payload": step.payload,
            })

    safe_name = re.sub(r'[^a-zA-Z0-9_-]', '_', seq.name)
    filename = f"{safe_name}_{seq.id}.json"

    return JSONResponse(
        content=payload,
        headers={"Content-Disposition": f'attachment; filename="{filename}"'},
        media_type="application/json"
    )


@router.post("/import-file", response_model=list[SequenceSchema])
async def import_sequences_file(file: UploadFile = File(...), db: AsyncSession = Depends(get_db)):
    repo = SequenceRepository(db)
    raw = await file.read()
    try:
        data = json.loads(raw)
    except Exception as e:
        raise HTTPException(400, f"Invalid JSON file: {e}")

    if "sequences" in data:
        seqs_data = data["sequences"]
    elif isinstance(data, list):
        seqs_data = data
    else:
        seqs_data = [data]

    try:
        parsed = [SequenceExportSchema.model_validate(x) for x in seqs_data]
    except ValidationError as e:
        raise HTTPException(400, f"Validation error: {e.errors()}")

    imported = []
    for seq in parsed:
        steps_data = []
        for step in seq.steps:
            channel_id = None
            if step.unit_id and step.channel_index is not None:
                # найти device по unit_id
                dev_res = await db.execute(
                    select(Device).where(Device.unit_id == step.unit_id)
                )
                device = dev_res.scalar_one_or_none()
                if device:
                    ch_res = await db.execute(
                        select(Channel).where(
                            Channel.device_id == device.id,
                            Channel.index == step.channel_index
                        )
                    )
                    channel = ch_res.scalar_one_or_none()
                    if channel:
                        channel_id = channel.id

            steps_data.append({
                "order_index": step.order_index,
                "kind": step.kind,
                "channel_id": channel_id,
                "payload": step.payload,
            })

        created = await repo.create(
            {"name": seq.name, "description": seq.description},
            steps_data,
        )
        imported.append(created)

    return imported
