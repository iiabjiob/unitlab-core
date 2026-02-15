from __future__ import annotations

import hashlib
import json
from datetime import datetime, timezone
from typing import Any

from fastapi import APIRouter, Depends, File, Form, HTTPException, UploadFile
from pydantic import ValidationError
from sqlalchemy.ext.asyncio import AsyncSession

from app.api.v1.signal_sheet import SignalSheetAutoAllocateResult, SignalSheetRepository
from app.api.v1.signals import SignalsRepository
from app.core.config import get_settings
from app.infrastructure.db.database import get_db
from app.schemas.signal_snapshot_schema import SignalImportMetaSchema
from app.schemas.signal_sheet_schema import (
    SignalAllocationBulkUpdateSchema,
    SignalAllocationRowSchema,
    SignalAutoAllocateResponseSchema,
    SignalAutoAllocateResultSchema,
    SignalAutoAllocateSchema,
    SignalSheetImportResponseSchema,
    SignalSheetPresetCreateSchema,
    SignalSheetPresetSchema,
    SignalSheetSchema,
)
from app.services.signal_sheet_import_service import SignalSheetImportService

router = APIRouter(prefix="/api/v1", tags=["Signal Sheet"])
settings = get_settings()


def get_repo(db: AsyncSession = Depends(get_db)) -> SignalSheetRepository:
    return SignalSheetRepository(db)


def get_signals_repo(db: AsyncSession = Depends(get_db)) -> SignalsRepository:
    return SignalsRepository(db)


@router.get("/workspaces/{workspace_id}/signal-sheet", response_model=SignalSheetSchema)
async def get_signal_sheet(
    workspace_id: int,
    repo: SignalSheetRepository = Depends(get_repo),
):
    if not await repo.ensure_workspace(workspace_id):
        raise HTTPException(status_code=404, detail="Workspace not found")

    sheet = await repo.get_sheet(workspace_id)
    return await _build_sheet_schema(repo, workspace_id, sheet)


@router.post("/workspaces/{workspace_id}/signal-sheet/import", response_model=SignalSheetImportResponseSchema)
async def import_signal_sheet(
    workspace_id: int,
    file: UploadFile = File(...),
    metadata: str | None = Form(default=None),
    preset_id: int | None = Form(default=None),
    save_preset_name: str | None = Form(default=None),
    repo: SignalSheetRepository = Depends(get_repo),
    signals_repo: SignalsRepository = Depends(get_signals_repo),
    db: AsyncSession = Depends(get_db),
):
    if not await repo.ensure_workspace(workspace_id):
        raise HTTPException(status_code=404, detail="Workspace not found")

    raw = await file.read()
    if not raw:
        raise HTTPException(status_code=400, detail="Uploaded file is empty")

    explicit_meta = _parse_metadata(metadata)
    preset_meta = None
    if preset_id is not None:
        preset = await repo.get_preset(preset_id)
        if preset is None or preset.workspace_id != workspace_id:
            raise HTTPException(status_code=404, detail="Preset not found")
        try:
            preset_meta = SignalImportMetaSchema.model_validate(preset.import_meta)
        except ValidationError:
            raise HTTPException(status_code=400, detail="Preset metadata is invalid")

    effective_meta = explicit_meta or preset_meta

    try:
        payload = SignalSheetImportService.parse_workbook(
            raw,
            filename=file.filename,
            metadata=effective_meta,
        )
    except Exception as exc:  # noqa: BLE001
        raise HTTPException(status_code=400, detail=f"Unable to parse workbook: {exc}")

    max_rows = max(1, int(settings.signal_import_max_rows))
    if payload.rows_count > max_rows:
        raise HTTPException(
            status_code=400,
            detail=f"Import limit exceeded: {payload.rows_count} rows (max {max_rows})",
        )

    await signals_repo.replace_from_import(workspace_id, payload.signals)
    await repo.cleanup_orphan_allocations(workspace_id)

    source_hash = hashlib.sha256(raw).hexdigest()
    await repo.upsert_sheet(
        workspace_id=workspace_id,
        source_filename=file.filename,
        source_hash=source_hash,
        rows_count=payload.rows_count,
        schema_version=2,
        data=payload.data,
        import_meta=effective_meta.model_dump() if effective_meta else None,
    )
    await db.commit()

    if save_preset_name and effective_meta is not None:
        await repo.save_preset(
            workspace_id=workspace_id,
            name=save_preset_name,
            import_meta=effective_meta,
        )

    sheet = await repo.get_sheet(workspace_id)
    response = SignalSheetImportResponseSchema(sheet=await _build_sheet_schema(repo, workspace_id, sheet))
    return response


@router.get("/workspaces/{workspace_id}/signal-sheet/presets", response_model=list[SignalSheetPresetSchema])
async def list_signal_sheet_presets(
    workspace_id: int,
    repo: SignalSheetRepository = Depends(get_repo),
):
    if not await repo.ensure_workspace(workspace_id):
        raise HTTPException(status_code=404, detail="Workspace not found")

    presets = await repo.list_presets(workspace_id)
    payload: list[SignalSheetPresetSchema] = []
    for item in presets:
        try:
            import_meta = SignalImportMetaSchema.model_validate(item.import_meta)
        except ValidationError:
            import_meta = SignalImportMetaSchema()
        payload.append(
            SignalSheetPresetSchema(
                id=item.id,
                workspace_id=item.workspace_id,
                name=item.name,
                import_meta=import_meta,
                created_at=item.created_at,
                updated_at=item.updated_at,
            )
        )
    return payload


@router.post("/workspaces/{workspace_id}/signal-sheet/presets", response_model=SignalSheetPresetSchema)
async def save_signal_sheet_preset(
    workspace_id: int,
    payload: SignalSheetPresetCreateSchema,
    repo: SignalSheetRepository = Depends(get_repo),
):
    if not await repo.ensure_workspace(workspace_id):
        raise HTTPException(status_code=404, detail="Workspace not found")

    try:
        preset = await repo.save_preset(
            workspace_id=workspace_id,
            name=payload.name,
            import_meta=payload.import_meta,
        )
    except ValueError as exc:
        raise HTTPException(status_code=400, detail=str(exc))

    return SignalSheetPresetSchema(
        id=preset.id,
        workspace_id=preset.workspace_id,
        name=preset.name,
        import_meta=SignalImportMetaSchema.model_validate(preset.import_meta),
        created_at=preset.created_at,
        updated_at=preset.updated_at,
    )


@router.delete("/signal-sheet/presets/{preset_id}")
async def delete_signal_sheet_preset(
    preset_id: int,
    repo: SignalSheetRepository = Depends(get_repo),
):
    deleted = await repo.delete_preset(preset_id)
    if not deleted:
        raise HTTPException(status_code=404, detail="Preset not found")
    return {"detail": "Preset deleted"}


@router.get("/workspaces/{workspace_id}/signal-allocations", response_model=list[SignalAllocationRowSchema])
async def list_signal_allocations(
    workspace_id: int,
    repo: SignalSheetRepository = Depends(get_repo),
):
    if not await repo.ensure_workspace(workspace_id):
        raise HTTPException(status_code=404, detail="Workspace not found")

    return await repo.list_allocation_rows(workspace_id)


@router.put("/workspaces/{workspace_id}/signal-allocations", response_model=list[SignalAllocationRowSchema])
async def update_signal_allocations(
    workspace_id: int,
    payload: SignalAllocationBulkUpdateSchema,
    repo: SignalSheetRepository = Depends(get_repo),
):
    if not await repo.ensure_workspace(workspace_id):
        raise HTTPException(status_code=404, detail="Workspace not found")

    try:
        await repo.update_allocations(
            workspace_id,
            [item.model_dump() for item in payload.entries],
        )
    except ValueError as exc:
        raise HTTPException(status_code=400, detail=str(exc))

    return await repo.list_allocation_rows(workspace_id)


@router.post("/workspaces/{workspace_id}/signal-allocations/auto", response_model=SignalAutoAllocateResponseSchema)
async def auto_allocate_signal_rows(
    workspace_id: int,
    payload: SignalAutoAllocateSchema,
    repo: SignalSheetRepository = Depends(get_repo),
):
    if not await repo.ensure_workspace(workspace_id):
        raise HTTPException(status_code=404, detail="Workspace not found")

    result: SignalSheetAutoAllocateResult = await repo.auto_allocate(
        workspace_id=workspace_id,
        signal_ids=payload.signal_ids,
        prefer_online=payload.prefer_online,
        overwrite_existing=payload.overwrite_existing,
    )

    rows = await repo.list_allocation_rows(workspace_id)
    return SignalAutoAllocateResponseSchema(
        result=SignalAutoAllocateResultSchema(
            assigned=result.assigned,
            skipped=result.skipped,
            missing=result.missing,
            unassigned_signal_ids=result.unassigned_signal_ids,
        ),
        rows=rows,
    )


def _parse_metadata(raw_metadata: str | None) -> SignalImportMetaSchema | None:
    if raw_metadata is None:
        return None

    try:
        payload: dict[str, Any] = json.loads(raw_metadata)
    except json.JSONDecodeError as exc:
        raise HTTPException(status_code=400, detail=f"Invalid metadata JSON: {exc}")

    try:
        return SignalImportMetaSchema.model_validate(payload)
    except ValidationError as exc:
        raise HTTPException(status_code=400, detail=exc.errors())


async def _build_sheet_schema(
    repo: SignalSheetRepository,
    workspace_id: int,
    sheet,
) -> SignalSheetSchema:
    signals_count = await repo.count_active_signals(workspace_id)
    allocated_count = await repo.count_allocated_signals(workspace_id)

    if sheet is None:
        return SignalSheetSchema(
            id=0,
            workspace_id=workspace_id,
            source_filename=None,
            source_hash=None,
            rows_count=0,
            schema_version=2,
            data={
                "version": 2,
                "sheet_count": 0,
                "default_sheet_index": 0,
                "sheets": [],
            },
            import_meta=None,
            signals_count=signals_count,
            allocated_count=allocated_count,
            created_at=datetime.now(timezone.utc),
            updated_at=datetime.now(timezone.utc),
        )

    import_meta = None
    if isinstance(sheet.import_meta, dict):
        try:
            import_meta = SignalImportMetaSchema.model_validate(sheet.import_meta)
        except ValidationError:
            import_meta = None

    return SignalSheetSchema(
        id=sheet.id,
        workspace_id=sheet.workspace_id,
        source_filename=sheet.source_filename,
        source_hash=sheet.source_hash,
        rows_count=sheet.rows_count,
        schema_version=sheet.schema_version,
        data=dict(sheet.data or {}),
        import_meta=import_meta,
        signals_count=signals_count,
        allocated_count=allocated_count,
        created_at=sheet.created_at,
        updated_at=sheet.updated_at,
    )
