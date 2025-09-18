import json, re
from fastapi import APIRouter, Depends, HTTPException, File, UploadFile
from sqlalchemy.ext.asyncio import AsyncSession
from fastapi.responses import JSONResponse
from pydantic import ValidationError
from app.infrastructure.db.database import get_db
from app.repositories.sequence_repository import (
    create_sequence, get_all_sequences, get_sequence,
    update_sequence, delete_sequence
)

from app.schemas.sequence_schema import (
    SequenceSchema, SequenceCreateSchema, SequenceUpdateSchema,
    SequenceExportSchema
)

router = APIRouter(prefix="/api/sequences", tags=["Sequences"])


@router.get("", response_model=list[SequenceSchema])
async def list_sequences(db: AsyncSession = Depends(get_db)):
    return await get_all_sequences(db)


@router.get("/{seq_id}", response_model=SequenceSchema)
async def get_one(seq_id: int, db: AsyncSession = Depends(get_db)):
    seq = await get_sequence(db, seq_id)
    if not seq:
        raise HTTPException(404, "Sequence not found")
    return seq


@router.post("", response_model=SequenceSchema)
async def create(data: SequenceCreateSchema, db: AsyncSession = Depends(get_db)):
    steps = [s.model_dump() for s in data.steps]
    return await create_sequence(db, data.model_dump(exclude={"steps"}), steps)


@router.patch("/{seq_id}", response_model=SequenceSchema)
async def update(seq_id: int, data: SequenceUpdateSchema, db: AsyncSession = Depends(get_db)):
    seq = await update_sequence(db, seq_id, data.model_dump(exclude_unset=True))
    if not seq:
        raise HTTPException(404, "Sequence not found")
    return seq


@router.delete("/{seq_id}")
async def delete(seq_id: int, db: AsyncSession = Depends(get_db)):
    ok = await delete_sequence(db, seq_id)
    if not ok:
        raise HTTPException(404, "Sequence not found")
    return {"detail": "Sequence deleted"}


@router.get("/{seq_id}/export-file")
async def export_sequence_file(seq_id: int, db: AsyncSession = Depends(get_db)):
    seq = await get_sequence(db, seq_id)
    if not seq:
        raise HTTPException(404, "Sequence not found")

    payload = {
        "name": seq.name,
        "description": seq.description,
        "steps": [
            {
                "order_index": step.order_index,
                "kind": step.kind,
                "unit_id": step.unit_id,
                "payload": step.payload,
            }
            for step in seq.steps
        ],
    }
    safe_name = re.sub(r'[^a-zA-Z0-9_-]', '_', seq.name)
    filename = f"{safe_name}_{seq.id}.json"

    return JSONResponse(
        content=payload,
        headers={"Content-Disposition": f'attachment; filename="{filename}"'},
        media_type="application/json"
    )


@router.post("/import-file", response_model=list[SequenceSchema])
async def import_sequences_file(file: UploadFile = File(...), db: AsyncSession = Depends(get_db)):
    raw = await file.read()
    try:
        data = json.loads(raw)
    except Exception as e:
        raise HTTPException(400, f"Invalid JSON file: {e}")

    # поддержка обертки {"sequences": [...]}
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
        created = await create_sequence(
            db,
            {"name": seq.name, "description": seq.description},
            [s.model_dump() for s in seq.steps],
        )
        imported.append(created)

    return imported