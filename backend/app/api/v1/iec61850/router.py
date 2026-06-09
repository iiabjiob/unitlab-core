from __future__ import annotations

from fastapi import APIRouter, Depends, File, Form, HTTPException, UploadFile
from fastapi.encoders import jsonable_encoder
from sqlalchemy.ext.asyncio import AsyncSession
from starlette.concurrency import run_in_threadpool

from app.infrastructure.db.database import get_db
from app.schemas.iec61850_scl_schema import (
    Iec61850RuntimeSelectionRequestSchema,
    Iec61850RuntimeSelectionResponseSchema,
    Iec61850SclImportResponseSchema,
)
from app.services.iec61850 import (
    Iec61850InMemorySclImportRepository,
    Iec61850SclImportError,
    Iec61850SclImportService,
    Iec61850SqlAlchemySclImportRepository,
    create_scl_cli_compiler_from_settings,
)
from app.services.iec61850.client_control import get_iec61850_client_control_service
from app.services.iec61850.report_runtime import Iec61850ReportRuntimeError

router = APIRouter(prefix="/api/v1/iec61850/client", tags=["IEC 61850 Client"])
scl_router = APIRouter(prefix="/api/v1/workspaces/{workspace_id}/iec61850", tags=["IEC 61850 SCL"])


@router.get("/state")
async def client_state() -> dict:
    return jsonable_encoder(get_iec61850_client_control_service().snapshot())


@router.get("/transcript")
async def client_transcript() -> dict:
    snapshot = get_iec61850_client_control_service().snapshot()
    return jsonable_encoder({"transcript": snapshot.transcript})


@router.post("/transcript/clear")
async def clear_client_transcript() -> dict:
    return jsonable_encoder(get_iec61850_client_control_service().clear_transcript())


@router.post("/session/open")
async def open_client_session() -> dict:
    return _run_action("open-session", get_iec61850_client_control_service().open_session)


@router.post("/session/close")
async def close_client_session() -> dict:
    return _run_action("close-session", get_iec61850_client_control_service().close_session)


@router.post("/report-control/read")
async def read_report_control() -> dict:
    return _run_action("read-report-control", get_iec61850_client_control_service().read_report_control)


@router.post("/report-control/reserve")
async def reserve_report_control() -> dict:
    return _run_action("reserve-report-control", get_iec61850_client_control_service().reserve_report_control)


@router.post("/report-control/enable")
async def enable_report_control() -> dict:
    return _run_action("enable-report-control", get_iec61850_client_control_service().enable_report_control)


@router.post("/report-control/gi")
async def send_general_interrogation() -> dict:
    return _run_action("send-general-interrogation", get_iec61850_client_control_service().send_general_interrogation)


@router.post("/report-control/disable")
async def disable_report_control() -> dict:
    return _run_action("disable-report-control", get_iec61850_client_control_service().disable_report_control)


@router.post("/report-control/release")
async def release_report_control() -> dict:
    return _run_action("release-report-control", get_iec61850_client_control_service().release_report_control)


@router.post("/subscription/run")
async def run_subscription_plan() -> dict:
    return _run_action("run-subscription-plan", get_iec61850_client_control_service().run_subscription_plan)


@router.post("/wire/start")
async def start_wire_transport() -> dict:
    return _run_action("wire-start", get_iec61850_client_control_service().start_live_wire_transport)


@router.post("/wire/emit-report")
async def emit_wire_report() -> dict:
    return _run_action("wire-emit-report", get_iec61850_client_control_service().emit_live_wire_report)


@router.post("/wire/stop")
async def stop_wire_transport() -> dict:
    return _run_action("wire-stop", get_iec61850_client_control_service().stop_live_wire_transport)


def _run_action(action: str, operation) -> dict:
    try:
        snapshot = operation()
    except Iec61850ReportRuntimeError as exc:
        raise HTTPException(
            status_code=409,
            detail={"action": action, "code": exc.code, "message": str(exc)},
        ) from exc
    return jsonable_encoder(snapshot)


@scl_router.post("/scl/import", response_model=Iec61850SclImportResponseSchema)
async def import_scl(
    workspace_id: int,
    file: UploadFile = File(...),
    selected_ied: str | None = Form(default=None),
    db: AsyncSession = Depends(get_db),
) -> Iec61850SclImportResponseSchema:
    repository = Iec61850SqlAlchemySclImportRepository(db)
    if not await repository.ensure_workspace(workspace_id):
        raise HTTPException(status_code=404, detail="Workspace not found")

    raw = await file.read()
    if not raw:
        raise HTTPException(status_code=400, detail="Uploaded SCL file is empty")

    try:
        service = Iec61850SclImportService(create_scl_cli_compiler_from_settings(), Iec61850InMemorySclImportRepository())
        prepared = await run_in_threadpool(
            service.prepare_import_record,
            workspace_id=workspace_id,
            source=raw,
            filename=file.filename,
            selected_ied=selected_ied,
        )
        saved = await repository.save(prepared, source=raw)
    except Iec61850SclImportError as exc:
        raise HTTPException(status_code=400, detail={"code": exc.code, "message": exc.message}) from exc

    return _scl_import_response(saved)


def _scl_import_response(record) -> Iec61850SclImportResponseSchema:
    return Iec61850SclImportResponseSchema(
        import_id=record.import_id,
        workspace_id=record.workspace_id,
        source_filename=record.source_filename,
        source_hash=record.source_hash,
        source_size=record.source_size,
        selected_ied=record.selected_ied,
        normalized_schema=record.normalized_schema,
        normalized_model=record.normalized_model,
        diagnostics=[
            {
                "severity": item.severity,
                "code": item.code,
                "message": item.message,
                "iedName": item.ied_name,
                "accessPointName": item.access_point_name,
                "logicalDeviceInst": item.logical_device_inst,
                "logicalNodeName": item.logical_node_name,
                "dataSetName": item.data_set_name,
                "reportControlName": item.report_control_name,
                "memberReference": item.member_reference,
            }
            for item in record.diagnostics
        ],
    )


@scl_router.get("/runtime/selection", response_model=Iec61850RuntimeSelectionResponseSchema | None)
async def get_runtime_selection(
    workspace_id: int,
    db: AsyncSession = Depends(get_db),
) -> Iec61850RuntimeSelectionResponseSchema | None:
    repository = Iec61850SqlAlchemySclImportRepository(db)
    if not await repository.ensure_workspace(workspace_id):
        raise HTTPException(status_code=404, detail="Workspace not found")
    selection = await repository.get_active_runtime_selection(workspace_id=workspace_id)
    return _runtime_selection_response(selection) if selection is not None else None


@scl_router.post("/runtime/selection", response_model=Iec61850RuntimeSelectionResponseSchema)
async def select_runtime_import(
    workspace_id: int,
    payload: Iec61850RuntimeSelectionRequestSchema,
    db: AsyncSession = Depends(get_db),
) -> Iec61850RuntimeSelectionResponseSchema:
    repository = Iec61850SqlAlchemySclImportRepository(db)
    if not await repository.ensure_workspace(workspace_id):
        raise HTTPException(status_code=404, detail="Workspace not found")
    try:
        selection = await repository.select_runtime_import(
            workspace_id=workspace_id,
            import_id=payload.import_id,
            selected_by=payload.selected_by,
            reason=payload.reason,
        )
    except Iec61850SclImportError as exc:
        raise HTTPException(status_code=404, detail={"code": exc.code, "message": exc.message}) from exc
    return _runtime_selection_response(selection)


def _runtime_selection_response(selection) -> Iec61850RuntimeSelectionResponseSchema:
    return Iec61850RuntimeSelectionResponseSchema(
        selection_id=selection.selection_id,
        workspace_id=selection.workspace_id,
        import_id=selection.import_id,
        runtime_revision=selection.runtime_revision,
        selected_ied=selection.selected_ied,
        source_hash=selection.source_hash,
        normalized_schema=selection.normalized_schema,
        selected_by=selection.selected_by,
        selection_reason=selection.selection_reason,
    )
