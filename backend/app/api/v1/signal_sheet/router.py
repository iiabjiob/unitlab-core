from __future__ import annotations

import json
from datetime import datetime, timezone
from typing import Any

from fastapi import APIRouter, Depends, File, Form, HTTPException, Query, UploadFile
from fastapi.responses import StreamingResponse
from pydantic import ValidationError
from sqlalchemy.ext.asyncio import AsyncSession

from app.api.v1.signal_sheet import SignalSheetAutoAllocateResult, SignalSheetRepository
from app.api.v1.signals import SignalsRepository
from app.core.config import get_settings
from app.core.logger import get_logger
from app.infrastructure.db.database import get_db
from app.schemas.signal_import_schema import SignalImportMetaSchema
from app.schemas.signal_sheet_schema import (
    SignalAllocationActionResponseSchema,
    SignalAllocationAssignActionSchema,
    SignalAllocationConflictSchema,
    SignalAllocationEnsureResponseSchema,
    SignalAllocationEnsureSchema,
    SignalJobControlSchema,
    SignalJobStatusSchema,
    SignalAllocationMarkTestedSchema,
    SignalAllocationReassignActionSchema,
    SignalAllocationRejectedItemSchema,
    SignalAllocationSwapActionSchema,
    SignalAllocationUnassignActionSchema,
    SignalTestRunJobSchema,
    SignalAllocationBulkUpdateSchema,
    SignalAllocationRowSchema,
    SignalAutoAllocateResponseSchema,
    SignalAutoAllocateResultSchema,
    SignalAutoAllocateSchema,
    SignalSheetImportResponseSchema,
    SignalSheetPresetCreateSchema,
    SignalSheetPresetSchema,
    SignalSheetSchema,
    SignalSheetImportPreviewResponseSchema,
    SignalSheetImportPreviewSheetSchema,
)
from app.services.signal_job_service import (
    control_signal_job,
    create_signal_job,
    get_signal_job,
)
from app.schemas.ws.events import build_signal_job_event
from app.core.events.ws_event_publisher import WsEventPublisher
from app.services.signal_sheet_import_service import SignalSheetImportService
from app.services.signal_sheet_write_service import SignalSheetWriteService

router = APIRouter(prefix="/api/v1", tags=["Signal Sheet"])
settings = get_settings()
logger = get_logger("api.signal_sheet")


def get_repo(db: AsyncSession = Depends(get_db)) -> SignalSheetRepository:
    return SignalSheetRepository(db)


def get_signals_repo(db: AsyncSession = Depends(get_db)) -> SignalsRepository:
    return SignalsRepository(db)


def get_write_service(
    db: AsyncSession = Depends(get_db),
    repo: SignalSheetRepository = Depends(get_repo),
    signals_repo: SignalsRepository = Depends(get_signals_repo),
) -> SignalSheetWriteService:
    return SignalSheetWriteService(db=db, repo=repo, signals_repo=signals_repo)


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
    write_service: SignalSheetWriteService = Depends(get_write_service),
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

    await write_service.import_sheet_from_parsed_payload(
        workspace_id=workspace_id,
        raw_file_bytes=raw,
        source_filename=file.filename,
        rows_count=payload.rows_count,
        parsed_data=payload.data,
        parsed_signals=payload.signals,
        import_meta=effective_meta,
        save_preset_name=save_preset_name,
    )

    sheet = await repo.get_sheet(workspace_id)
    response = SignalSheetImportResponseSchema(sheet=await _build_sheet_schema(repo, workspace_id, sheet))
    return response


@router.post(
    "/workspaces/{workspace_id}/signal-sheet/import/preview",
    response_model=SignalSheetImportPreviewResponseSchema,
)
async def preview_signal_sheet_import(
    workspace_id: int,
    file: UploadFile = File(...),
    metadata: str | None = Form(default=None),
    preset_id: int | None = Form(default=None),
    repo: SignalSheetRepository = Depends(get_repo),
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

    raw_data = payload.data if isinstance(payload.data, dict) else {}
    raw_sheets = raw_data.get("sheets") if isinstance(raw_data.get("sheets"), list) else []

    sheets: list[SignalSheetImportPreviewSheetSchema] = []
    for item in raw_sheets:
        if not isinstance(item, dict):
            continue
        rows = item.get("rows") if isinstance(item.get("rows"), list) else []
        normalized_rows = [row for row in rows if isinstance(row, dict)]
        sheets.append(
            SignalSheetImportPreviewSheetSchema(
                name=str(item.get("name") or "Sheet"),
                index=int(item.get("index") or 0),
                headers=[str(header) for header in (item.get("headers") or []) if str(header).strip()],
                rows_count=int(item.get("rows_count") or len(normalized_rows)),
                rows=normalized_rows,
            )
        )

    return SignalSheetImportPreviewResponseSchema(
        rows_count=payload.rows_count,
        sheet_count=int(raw_data.get("sheet_count") or len(sheets)),
        default_sheet_index=int(raw_data.get("default_sheet_index") or 0),
        sheets=sheets,
    )


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
    write_service: SignalSheetWriteService = Depends(get_write_service),
):
    if not await repo.ensure_workspace(workspace_id):
        raise HTTPException(status_code=404, detail="Workspace not found")

    try:
        preset = await write_service.save_preset(
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
    write_service: SignalSheetWriteService = Depends(get_write_service),
):
    deleted = await write_service.delete_preset(preset_id)
    if not deleted:
        raise HTTPException(status_code=404, detail="Preset not found")
    return {"detail": "Preset deleted"}


@router.get("/workspaces/{workspace_id}/signal-allocations", response_model=list[SignalAllocationRowSchema])
async def list_signal_allocations(
    workspace_id: int,
    offset: int = Query(default=0, ge=0),
    limit: int = Query(default=0, ge=0, le=5000),
    repo: SignalSheetRepository = Depends(get_repo),
):
    if not await repo.ensure_workspace(workspace_id):
        raise HTTPException(status_code=404, detail="Workspace not found")

    if limit > 0:
        return await repo.list_allocation_rows_page(
            workspace_id,
            offset=offset,
            limit=limit,
        )

    return await repo.list_allocation_rows(workspace_id)


@router.get("/workspaces/{workspace_id}/signal-allocations.ndjson")
async def stream_signal_allocations_ndjson(
    workspace_id: int,
    repo: SignalSheetRepository = Depends(get_repo),
):
    if not await repo.ensure_workspace(workspace_id):
        raise HTTPException(status_code=404, detail="Workspace not found")

    async def _iter_lines():
        offset = 0
        page_size = 500
        while True:
            rows = await repo.list_allocation_rows_page(
                workspace_id,
                offset=offset,
                limit=page_size,
            )
            if not rows:
                break

            for row in rows:
                yield row.model_dump_json() + "\n"

            if len(rows) < page_size:
                break
            offset += len(rows)

    return StreamingResponse(
        _iter_lines(),
        media_type="application/x-ndjson; charset=utf-8",
        headers={"Cache-Control": "no-store"},
    )


@router.put("/workspaces/{workspace_id}/signal-allocations", response_model=SignalAllocationActionResponseSchema)
async def update_signal_allocations(
    workspace_id: int,
    payload: SignalAllocationBulkUpdateSchema,
    repo: SignalSheetRepository = Depends(get_repo),
    write_service: SignalSheetWriteService = Depends(get_write_service),
):
    if not await repo.ensure_workspace(workspace_id):
        raise HTTPException(status_code=404, detail="Workspace not found")

    try:
        await write_service.update_allocations(workspace_id, [item.model_dump() for item in payload.entries])
    except ValueError as exc:
        raise HTTPException(status_code=400, detail=str(exc))

    touched_signal_ids = [item.signal_id for item in payload.entries]
    rows = await repo.list_allocation_rows_by_signal_ids(workspace_id, touched_signal_ids)
    return SignalAllocationActionResponseSchema(
        workspace_id=workspace_id,
        changed_rows=rows,
        conflicts=[],
        rejected=[],
    )


@router.post(
    "/workspaces/{workspace_id}/signal-allocations/actions/assign",
    response_model=SignalAllocationActionResponseSchema,
)
async def assign_signal_allocation(
    workspace_id: int,
    payload: SignalAllocationAssignActionSchema,
    repo: SignalSheetRepository = Depends(get_repo),
    write_service: SignalSheetWriteService = Depends(get_write_service),
):
    if not await repo.ensure_workspace(workspace_id):
        raise HTTPException(status_code=404, detail="Workspace not found")

    try:
        changed_signal_ids = await write_service.assign_allocation(
            workspace_id=workspace_id,
            signal_id=payload.signal_id,
            channel_id=payload.channel_id,
            allocation_meta=payload.allocation_meta,
        )
    except ValueError as exc:
        raise _allocation_action_http_exception(exc, signal_id=payload.signal_id, channel_id=payload.channel_id)

    return await _build_allocation_action_response(repo, workspace_id, changed_signal_ids)


@router.post(
    "/workspaces/{workspace_id}/signal-allocations/actions/reassign",
    response_model=SignalAllocationActionResponseSchema,
)
async def reassign_signal_allocation(
    workspace_id: int,
    payload: SignalAllocationReassignActionSchema,
    repo: SignalSheetRepository = Depends(get_repo),
    write_service: SignalSheetWriteService = Depends(get_write_service),
):
    if not await repo.ensure_workspace(workspace_id):
        raise HTTPException(status_code=404, detail="Workspace not found")

    try:
        changed_signal_ids = await write_service.reassign_allocation(
            workspace_id=workspace_id,
            signal_id=payload.signal_id,
            channel_id=payload.channel_id,
            allocation_meta=payload.allocation_meta,
        )
    except ValueError as exc:
        raise _allocation_action_http_exception(exc, signal_id=payload.signal_id, channel_id=payload.channel_id)

    return await _build_allocation_action_response(repo, workspace_id, changed_signal_ids)


@router.post(
    "/workspaces/{workspace_id}/signal-allocations/actions/unassign",
    response_model=SignalAllocationActionResponseSchema,
)
async def unassign_signal_allocation(
    workspace_id: int,
    payload: SignalAllocationUnassignActionSchema,
    repo: SignalSheetRepository = Depends(get_repo),
    write_service: SignalSheetWriteService = Depends(get_write_service),
):
    if not await repo.ensure_workspace(workspace_id):
        raise HTTPException(status_code=404, detail="Workspace not found")

    try:
        changed_signal_ids = await write_service.unassign_allocation(
            workspace_id=workspace_id,
            signal_id=payload.signal_id,
        )
    except ValueError as exc:
        raise _allocation_action_http_exception(exc, signal_id=payload.signal_id, channel_id=None)

    return await _build_allocation_action_response(repo, workspace_id, changed_signal_ids)


@router.post(
    "/workspaces/{workspace_id}/signal-allocations/actions/swap",
    response_model=SignalAllocationActionResponseSchema,
)
async def swap_signal_allocations(
    workspace_id: int,
    payload: SignalAllocationSwapActionSchema,
    repo: SignalSheetRepository = Depends(get_repo),
    write_service: SignalSheetWriteService = Depends(get_write_service),
):
    if not await repo.ensure_workspace(workspace_id):
        raise HTTPException(status_code=404, detail="Workspace not found")

    try:
        changed_signal_ids = await write_service.swap_allocations(
            workspace_id=workspace_id,
            signal_id=payload.signal_id,
            channel_id=payload.channel_id,
        )
    except ValueError as exc:
        raise _allocation_action_http_exception(exc, signal_id=payload.signal_id, channel_id=payload.channel_id)

    return await _build_allocation_action_response(repo, workspace_id, changed_signal_ids)


@router.post("/workspaces/{workspace_id}/signal-allocations/auto", response_model=SignalAutoAllocateResponseSchema)
async def auto_allocate_signal_rows(
    workspace_id: int,
    payload: SignalAutoAllocateSchema,
    repo: SignalSheetRepository = Depends(get_repo),
    write_service: SignalSheetWriteService = Depends(get_write_service),
):
    if not await repo.ensure_workspace(workspace_id):
        raise HTTPException(status_code=404, detail="Workspace not found")

    try:
        result: SignalSheetAutoAllocateResult = await write_service.auto_allocate(
            workspace_id=workspace_id,
            signal_ids=payload.signal_ids,
            prefer_online=payload.prefer_online,
            prefer_single_unit=payload.prefer_single_unit,
            overwrite_existing=payload.overwrite_existing,
        )
    except ValueError as exc:
        raise HTTPException(status_code=400, detail=str(exc))

    rows = await repo.list_allocation_rows_by_signal_ids(workspace_id, result.changed_signal_ids)
    return SignalAutoAllocateResponseSchema(
        result=SignalAutoAllocateResultSchema(
            assigned=result.assigned,
            skipped=result.skipped,
            missing=result.missing,
            unassigned_signal_ids=result.unassigned_signal_ids,
        ),
        changed_rows=rows,
        skipped=result.skipped_items,
        rejected=result.rejected,
    )


@router.post("/workspaces/{workspace_id}/signal-allocations/auto/jobs", response_model=SignalJobStatusSchema)
async def enqueue_auto_allocate_signal_rows(
    workspace_id: int,
    payload: SignalAutoAllocateSchema,
    repo: SignalSheetRepository = Depends(get_repo),
):
    if not await repo.ensure_workspace(workspace_id):
        raise HTTPException(status_code=404, detail="Workspace not found")

    job_state = await create_signal_job(
        workspace_id=workspace_id,
        operation="auto_allocate",
        payload=payload.model_dump(),
    )
    logger.info(
        "🧰 Enqueued signal allocation job | workspace=%s op=auto_allocate mode=async job_id=%s requested=%s",
        workspace_id,
        str(job_state.get("job_id") or ""),
        len(payload.signal_ids or []),
    )
    await WsEventPublisher.publish(build_signal_job_event(job_state))
    return SignalJobStatusSchema.model_validate(job_state)


@router.post("/workspaces/{workspace_id}/signal-allocations/jobs", response_model=SignalJobStatusSchema)
async def enqueue_bulk_signal_allocations_update(
    workspace_id: int,
    payload: SignalAllocationBulkUpdateSchema,
    repo: SignalSheetRepository = Depends(get_repo),
):
    if not await repo.ensure_workspace(workspace_id):
        raise HTTPException(status_code=404, detail="Workspace not found")

    job_state = await create_signal_job(
        workspace_id=workspace_id,
        operation="bulk_update",
        payload=payload.model_dump(),
    )
    logger.info(
        "🧰 Enqueued signal allocation job | workspace=%s op=bulk_update mode=async job_id=%s entries=%s",
        workspace_id,
        str(job_state.get("job_id") or ""),
        len(payload.entries or []),
    )
    await WsEventPublisher.publish(build_signal_job_event(job_state))
    return SignalJobStatusSchema.model_validate(job_state)


@router.post("/workspaces/{workspace_id}/signal-allocations/test-run/jobs", response_model=SignalJobStatusSchema)
async def enqueue_signal_test_run_job(
    workspace_id: int,
    payload: SignalTestRunJobSchema,
    repo: SignalSheetRepository = Depends(get_repo),
):
    if not await repo.ensure_workspace(workspace_id):
        raise HTTPException(status_code=404, detail="Workspace not found")

    if len(payload.signal_ids) > settings.signal_test_run_max_signals:
        raise HTTPException(
            status_code=400,
            detail=f"Too many signals for test run (max {settings.signal_test_run_max_signals})",
        )

    try:
        job_state = await create_signal_job(
            workspace_id=workspace_id,
            operation="test_run",
            payload=payload.model_dump(),
        )
    except RuntimeError as exc:
        raise HTTPException(status_code=409, detail=str(exc)) from exc
    await WsEventPublisher.publish(build_signal_job_event(job_state))
    return SignalJobStatusSchema.model_validate(job_state)


@router.get("/workspaces/{workspace_id}/signal-allocation-jobs/{job_id}", response_model=SignalJobStatusSchema)
async def get_signal_job_status(
    workspace_id: int,
    job_id: str,
    repo: SignalSheetRepository = Depends(get_repo),
):
    if not await repo.ensure_workspace(workspace_id):
        raise HTTPException(status_code=404, detail="Workspace not found")

    job_state = await get_signal_job(job_id)
    if job_state is None:
        raise HTTPException(status_code=404, detail="Job not found")
    if int(job_state.get("workspace_id") or 0) != workspace_id:
        raise HTTPException(status_code=404, detail="Job not found")

    return SignalJobStatusSchema.model_validate(job_state)


@router.post("/workspaces/{workspace_id}/signal-allocation-jobs/{job_id}/control", response_model=SignalJobStatusSchema)
async def control_signal_job_status(
    workspace_id: int,
    job_id: str,
    payload: SignalJobControlSchema,
    repo: SignalSheetRepository = Depends(get_repo),
):
    if not await repo.ensure_workspace(workspace_id):
        raise HTTPException(status_code=404, detail="Workspace not found")

    job_state = await get_signal_job(job_id)
    if job_state is None:
        raise HTTPException(status_code=404, detail="Job not found")
    if int(job_state.get("workspace_id") or 0) != workspace_id:
        raise HTTPException(status_code=404, detail="Job not found")

    action = payload.action
    if action not in {"pause", "resume", "stop"}:
        raise HTTPException(status_code=400, detail="Unsupported action")

    controlled = await control_signal_job(job_id, action)
    if controlled is None:
        raise HTTPException(status_code=404, detail="Job not found")

    await WsEventPublisher.publish(build_signal_job_event(controlled))
    return SignalJobStatusSchema.model_validate(controlled)


@router.post("/workspaces/{workspace_id}/signal-allocations/ensure", response_model=SignalAllocationEnsureResponseSchema)
async def ensure_signal_allocations(
    workspace_id: int,
    payload: SignalAllocationEnsureSchema,
    repo: SignalSheetRepository = Depends(get_repo),
    write_service: SignalSheetWriteService = Depends(get_write_service),
):
    if not await repo.ensure_workspace(workspace_id):
        raise HTTPException(status_code=404, detail="Workspace not found")

    signal_ids = [int(item) for item in payload.signal_ids if int(item) > 0]
    if not signal_ids:
        return SignalAllocationEnsureResponseSchema(
            result=SignalAutoAllocateResultSchema(
                assigned=0,
                skipped=0,
                missing=0,
                unassigned_signal_ids=[],
            ),
            rows=[],
        )

    try:
        result: SignalSheetAutoAllocateResult = await write_service.auto_allocate(
            workspace_id=workspace_id,
            signal_ids=signal_ids,
            prefer_online=payload.prefer_online,
            prefer_single_unit=False,
            overwrite_existing=False,
        )
    except ValueError as exc:
        raise HTTPException(status_code=400, detail=str(exc))
    rows = await repo.list_allocation_rows_by_signal_ids(workspace_id, signal_ids)
    return SignalAllocationEnsureResponseSchema(
        result=SignalAutoAllocateResultSchema(
            assigned=result.assigned,
            skipped=result.skipped,
            missing=result.missing,
            unassigned_signal_ids=result.unassigned_signal_ids,
        ),
        rows=rows,
    )


@router.post("/workspaces/{workspace_id}/signal-allocations/tested", response_model=list[SignalAllocationRowSchema])
async def mark_signal_allocations_tested(
    workspace_id: int,
    payload: SignalAllocationMarkTestedSchema,
    repo: SignalSheetRepository = Depends(get_repo),
    write_service: SignalSheetWriteService = Depends(get_write_service),
):
    if not await repo.ensure_workspace(workspace_id):
        raise HTTPException(status_code=404, detail="Workspace not found")

    signal_ids = await write_service.mark_signals_tested(workspace_id, payload.signal_ids)
    if not signal_ids:
        return []
    return await repo.list_allocation_rows_by_signal_ids(workspace_id, signal_ids)


async def _build_allocation_action_response(
    repo: SignalSheetRepository,
    workspace_id: int,
    changed_signal_ids: list[int],
) -> SignalAllocationActionResponseSchema:
    rows = await repo.list_allocation_rows_by_signal_ids(workspace_id, changed_signal_ids)
    return SignalAllocationActionResponseSchema(
        workspace_id=workspace_id,
        changed_rows=rows,
        conflicts=[],
        rejected=[],
    )


def _allocation_action_http_exception(
    exc: ValueError,
    *,
    signal_id: int | None,
    channel_id: int | None,
) -> HTTPException:
    message = str(exc)
    normalized = message.lower()
    conflict_markers = (
        "already allocated",
        "already assigned",
        "already allocated to another signal",
        "use reassign",
        "use assign",
    )
    status_code = 409 if any(marker in normalized for marker in conflict_markers) else 400
    code = "allocation_conflict" if status_code == 409 else "allocation_rejected"
    detail = {
        "message": message,
        "conflicts": [
            SignalAllocationConflictSchema(
                code=code,
                message=message,
                signal_id=signal_id,
                channel_id=channel_id,
            ).model_dump()
        ] if status_code == 409 else [],
        "rejected": [
            SignalAllocationRejectedItemSchema(
                code=code,
                message=message,
                signal_id=signal_id,
                channel_id=channel_id,
            ).model_dump()
        ],
    }
    return HTTPException(status_code=status_code, detail=detail)


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


def _compact_sheet_payload(raw_data: Any) -> dict[str, Any]:
    if not isinstance(raw_data, dict):
        return {
            "version": 2,
            "sheet_count": 0,
            "default_sheet_index": 0,
            "sheets": [],
        }

    raw_sheets = raw_data.get("sheets")
    compact_sheets: list[dict[str, Any]] = []
    if isinstance(raw_sheets, list):
        for item in raw_sheets:
            if not isinstance(item, dict):
                continue
            compact_sheets.append(
                {
                    "name": item.get("name"),
                    "index": item.get("index"),
                    "headers": item.get("headers") if isinstance(item.get("headers"), list) else [],
                    "rows_count": item.get("rows_count"),
                }
            )

    return {
        "version": raw_data.get("version", 2),
        "sheet_count": raw_data.get("sheet_count", len(compact_sheets)),
        "default_sheet_index": raw_data.get("default_sheet_index", 0),
        "sheets": compact_sheets,
    }


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
        data=_compact_sheet_payload(sheet.data),
        import_meta=import_meta,
        signals_count=signals_count,
        allocated_count=allocated_count,
        created_at=sheet.created_at,
        updated_at=sheet.updated_at,
    )
