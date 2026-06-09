from __future__ import annotations

import asyncio
import json
import uuid
from fastapi import APIRouter, Depends, File, Form, HTTPException, UploadFile
from fastapi.encoders import jsonable_encoder
from sqlalchemy.ext.asyncio import AsyncSession
from starlette.concurrency import run_in_threadpool

from app.infrastructure.db.database import AsyncSessionLocal, get_db
from app.schemas.iec61850_scl_schema import (
    Iec61850RuntimeSelectionRequestSchema,
    Iec61850RuntimeSelectionResponseSchema,
    Iec61850VirtualMmsServerStartRequestSchema,
    Iec61850VirtualMmsServerStateSchema,
    Iec61850SclImportBatchJobStartResponseSchema,
    Iec61850SclImportBatchJobStatusSchema,
    Iec61850SclImportBatchResponseSchema,
    Iec61850SclImportListResponseSchema,
    Iec61850SclImportResponseSchema,
    Iec61850SclIedDiscoveryResponseSchema,
)
from app.core.logger import get_logger
from app.services.iec61850 import (
    Iec61850InMemorySclImportRepository,
    Iec61850SclImportError,
    Iec61850SclImportService,
    Iec61850SqlAlchemySclImportRepository,
    create_scl_cli_compiler_from_settings,
)
from app.services.iec61850.client_control import get_iec61850_client_control_service
from app.services.iec61850.report_runtime import Iec61850ReportRuntimeError
from app.services.iec61850.virtual_mms_server import get_virtual_mms_server_service

router = APIRouter(prefix="/api/v1/iec61850/client", tags=["IEC 61850 Client"])
scl_router = APIRouter(prefix="/api/v1/workspaces/{workspace_id}/iec61850", tags=["IEC 61850 SCL"])
logger = get_logger("api.iec61850")
_scl_import_batch_jobs: dict[str, dict] = {}


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


def _parse_selected_ieds_payload(selected_ieds: str) -> list[str]:
    try:
        parsed_ieds = json.loads(selected_ieds)
    except json.JSONDecodeError as exc:
        raise HTTPException(status_code=400, detail={"code": "SCL_SELECTED_IEDS_INVALID", "message": "selected_ieds must be a JSON array."}) from exc
    if not isinstance(parsed_ieds, list):
        raise HTTPException(status_code=400, detail={"code": "SCL_SELECTED_IEDS_INVALID", "message": "selected_ieds must be a JSON array."})

    requested_ieds = []
    seen_ieds = set()
    for item in parsed_ieds:
        name = str(item).strip()
        if not name or name in seen_ieds:
            continue
        requested_ieds.append(name)
        seen_ieds.add(name)
    if not requested_ieds:
        raise HTTPException(status_code=400, detail={"code": "SCL_SELECTED_IEDS_EMPTY", "message": "At least one selected IED is required."})
    return requested_ieds


def _batch_job_status_response(job_id: str, job: dict) -> Iec61850SclImportBatchJobStatusSchema:
    include_imports = job["status"] in {"completed", "failed", "cancelled"}
    return Iec61850SclImportBatchJobStatusSchema(
        job_id=job_id,
        status=job["status"],
        workspace_id=job["workspace_id"],
        source_filename=job.get("source_filename"),
        total=job["total"],
        current=job["current"],
        current_ied=job.get("current_ied"),
        imports=job["imports"] if include_imports else [],
        failures=job["failures"],
        message=job.get("message"),
    )


async def _run_scl_import_batch_job(job_id: str, raw: bytes, selected_ieds: list[str]) -> None:
    job = _scl_import_batch_jobs[job_id]
    logger.info("scl-import-batch-job-start job=%s workspace=%s selected=%s", job_id, job["workspace_id"], len(selected_ieds))
    try:
        async with AsyncSessionLocal() as db:
            repository = Iec61850SqlAlchemySclImportRepository(db)
            service = Iec61850SclImportService(create_scl_cli_compiler_from_settings(), Iec61850InMemorySclImportRepository())
            for index, selected_ied in enumerate(selected_ieds, start=1):
                if job.get("cancel_requested"):
                    job["status"] = "cancelled"
                    job["message"] = "Batch compile cancelled."
                    logger.info("scl-import-batch-job-cancelled job=%s current=%s total=%s", job_id, job["current"], job["total"])
                    return
                job["status"] = "running"
                job["current"] = index - 1
                job["current_ied"] = selected_ied
                logger.info("scl-import-batch-job-compile job=%s progress=%s/%s ied=%s", job_id, index, len(selected_ieds), selected_ied)
                try:
                    prepared = await run_in_threadpool(
                        service.prepare_import_record,
                        workspace_id=job["workspace_id"],
                        source=raw,
                        filename=job.get("source_filename"),
                        selected_ied=selected_ied,
                    )
                    saved = await repository.save(prepared, source=raw)
                    job["imports"].append(_scl_import_response(saved))
                    logger.info("scl-import-batch-job-success job=%s ied=%s import_id=%s", job_id, selected_ied, saved.import_id)
                except Iec61850SclImportError as exc:
                    failure = {"selected_ied": selected_ied, "code": exc.code, "message": exc.message}
                    job["failures"].append(failure)
                    logger.warning("scl-import-batch-job-failure job=%s ied=%s code=%s message=%s", job_id, selected_ied, exc.code, exc.message)
                finally:
                    job["current"] = index
            job["status"] = "completed"
            job["current_ied"] = None
            logger.info("scl-import-batch-job-complete job=%s imports=%s failures=%s", job_id, len(job["imports"]), len(job["failures"]))
    except Exception as exc:
        job["status"] = "failed"
        job["message"] = str(exc)
        logger.exception("scl-import-batch-job-error job=%s", job_id)


@scl_router.post("/scl/ieds", response_model=Iec61850SclIedDiscoveryResponseSchema)
async def discover_scl_ieds(
    workspace_id: int,
    file: UploadFile = File(...),
    db: AsyncSession = Depends(get_db),
) -> Iec61850SclIedDiscoveryResponseSchema:
    repository = Iec61850SqlAlchemySclImportRepository(db)
    if not await repository.ensure_workspace(workspace_id):
        raise HTTPException(status_code=404, detail="Workspace not found")

    raw = await file.read()
    if not raw:
        raise HTTPException(status_code=400, detail="Uploaded SCL file is empty")

    try:
        service = Iec61850SclImportService(create_scl_cli_compiler_from_settings(), Iec61850InMemorySclImportRepository())
        discovered = await run_in_threadpool(service.discover_ieds, source=raw)
    except Iec61850SclImportError as exc:
        raise HTTPException(status_code=400, detail={"code": exc.code, "message": exc.message}) from exc

    return Iec61850SclIedDiscoveryResponseSchema(
        schema=discovered.schema,
        sourceSize=discovered.source_size,
        ieds=[{"name": item.name, "accessPointCount": item.access_point_count} for item in discovered.ieds],
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
            for item in discovered.diagnostics
        ],
    )


@scl_router.post("/scl/import-batch/jobs", response_model=Iec61850SclImportBatchJobStartResponseSchema)
async def start_scl_import_batch_job(
    workspace_id: int,
    file: UploadFile = File(...),
    selected_ieds: str = Form(...),
    db: AsyncSession = Depends(get_db),
) -> Iec61850SclImportBatchJobStartResponseSchema:
    repository = Iec61850SqlAlchemySclImportRepository(db)
    if not await repository.ensure_workspace(workspace_id):
        raise HTTPException(status_code=404, detail="Workspace not found")
    raw = await file.read()
    if not raw:
        raise HTTPException(status_code=400, detail="Uploaded SCL file is empty")
    requested_ieds = _parse_selected_ieds_payload(selected_ieds)
    job_id = uuid.uuid4().hex
    _scl_import_batch_jobs[job_id] = {
        "status": "queued",
        "workspace_id": workspace_id,
        "source_filename": file.filename,
        "total": len(requested_ieds),
        "current": 0,
        "current_ied": None,
        "imports": [],
        "failures": [],
        "message": None,
        "cancel_requested": False,
    }
    asyncio.create_task(_run_scl_import_batch_job(job_id, raw, requested_ieds))
    return Iec61850SclImportBatchJobStartResponseSchema(job_id=job_id, total=len(requested_ieds))


@scl_router.get("/scl/import-batch/jobs/{job_id}", response_model=Iec61850SclImportBatchJobStatusSchema)
async def get_scl_import_batch_job(workspace_id: int, job_id: str) -> Iec61850SclImportBatchJobStatusSchema:
    job = _scl_import_batch_jobs.get(job_id)
    if job is None or job["workspace_id"] != workspace_id:
        raise HTTPException(status_code=404, detail="SCL import batch job not found")
    return _batch_job_status_response(job_id, job)


@scl_router.post("/scl/import-batch/jobs/{job_id}/cancel", response_model=Iec61850SclImportBatchJobStatusSchema)
async def cancel_scl_import_batch_job(workspace_id: int, job_id: str) -> Iec61850SclImportBatchJobStatusSchema:
    job = _scl_import_batch_jobs.get(job_id)
    if job is None or job["workspace_id"] != workspace_id:
        raise HTTPException(status_code=404, detail="SCL import batch job not found")
    if job["status"] in {"queued", "running"}:
        job["cancel_requested"] = True
        job["message"] = "Cancel requested."
    return _batch_job_status_response(job_id, job)


@scl_router.post("/scl/import-batch", response_model=Iec61850SclImportBatchResponseSchema)
async def import_scl_batch(
    workspace_id: int,
    file: UploadFile = File(...),
    selected_ieds: str = Form(...),
    db: AsyncSession = Depends(get_db),
) -> Iec61850SclImportBatchResponseSchema:
    repository = Iec61850SqlAlchemySclImportRepository(db)
    if not await repository.ensure_workspace(workspace_id):
        raise HTTPException(status_code=404, detail="Workspace not found")

    raw = await file.read()
    if not raw:
        raise HTTPException(status_code=400, detail="Uploaded SCL file is empty")

    requested_ieds = _parse_selected_ieds_payload(selected_ieds)

    logger.info(
        "scl-import-batch-start workspace=%s file=%s bytes=%s selected=%s",
        workspace_id,
        file.filename,
        len(raw),
        len(requested_ieds),
    )
    service = Iec61850SclImportService(create_scl_cli_compiler_from_settings(), Iec61850InMemorySclImportRepository())
    imports = []
    failures = []
    for index, selected_ied in enumerate(requested_ieds, start=1):
        logger.info("scl-import-batch-compile workspace=%s progress=%s/%s ied=%s", workspace_id, index, len(requested_ieds), selected_ied)
        try:
            prepared = await run_in_threadpool(
                service.prepare_import_record,
                workspace_id=workspace_id,
                source=raw,
                filename=file.filename,
                selected_ied=selected_ied,
            )
            saved = await repository.save(prepared, source=raw)
            imports.append(_scl_import_response(saved))
            logger.info("scl-import-batch-success workspace=%s ied=%s import_id=%s", workspace_id, selected_ied, saved.import_id)
        except Iec61850SclImportError as exc:
            failures.append({"selected_ied": selected_ied, "code": exc.code, "message": exc.message})
            logger.warning("scl-import-batch-failure workspace=%s ied=%s code=%s message=%s", workspace_id, selected_ied, exc.code, exc.message)

    logger.info("scl-import-batch-complete workspace=%s imports=%s failures=%s", workspace_id, len(imports), len(failures))
    return Iec61850SclImportBatchResponseSchema(
        workspace_id=workspace_id,
        source_filename=file.filename,
        source_size=len(raw),
        imports=imports,
        failures=failures,
    )


@scl_router.get("/scl/imports", response_model=Iec61850SclImportListResponseSchema)
async def list_scl_imports(
    workspace_id: int,
    limit: int = 100,
    db: AsyncSession = Depends(get_db),
) -> Iec61850SclImportListResponseSchema:
    repository = Iec61850SqlAlchemySclImportRepository(db)
    if not await repository.ensure_workspace(workspace_id):
        raise HTTPException(status_code=404, detail="Workspace not found")
    imports = await repository.list_imports(workspace_id=workspace_id, limit=limit)
    return Iec61850SclImportListResponseSchema(
        workspace_id=workspace_id,
        imports=[_scl_import_response(record) for record in imports],
    )


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



@scl_router.get("/virtual-mms-server", response_model=Iec61850VirtualMmsServerStateSchema)
async def get_virtual_mms_server_state(workspace_id: int) -> Iec61850VirtualMmsServerStateSchema:
    _ = workspace_id
    return _virtual_mms_server_response(get_virtual_mms_server_service().snapshot())


@scl_router.post("/virtual-mms-server/start", response_model=Iec61850VirtualMmsServerStateSchema)
async def start_virtual_mms_server(
    workspace_id: int,
    payload: Iec61850VirtualMmsServerStartRequestSchema,
    db: AsyncSession = Depends(get_db),
) -> Iec61850VirtualMmsServerStateSchema:
    repository = Iec61850SqlAlchemySclImportRepository(db)
    if not await repository.ensure_workspace(workspace_id):
        raise HTTPException(status_code=404, detail="Workspace not found")
    record = await repository.get_import(workspace_id=workspace_id, import_id=payload.import_id)
    source = await repository.get_import_source(workspace_id=workspace_id, import_id=payload.import_id)
    if record is None or source is None:
        raise HTTPException(status_code=404, detail={"code": "SCL_IMPORT_NOT_FOUND", "message": "SCL import was not found for this workspace."})
    try:
        snapshot = get_virtual_mms_server_service().start(record, source_bytes=source, host=payload.host, port=payload.port)
    except Iec61850ReportRuntimeError as exc:
        raise HTTPException(status_code=409, detail={"code": exc.code, "message": exc.message}) from exc
    return _virtual_mms_server_response(snapshot)


@scl_router.post("/virtual-mms-server/stop", response_model=Iec61850VirtualMmsServerStateSchema)
async def stop_virtual_mms_server(workspace_id: int) -> Iec61850VirtualMmsServerStateSchema:
    _ = workspace_id
    return _virtual_mms_server_response(get_virtual_mms_server_service().stop())


def _virtual_mms_server_response(snapshot) -> Iec61850VirtualMmsServerStateSchema:
    return Iec61850VirtualMmsServerStateSchema(
        running=snapshot.running,
        import_id=snapshot.import_id,
        selected_ied=snapshot.selected_ied,
        source_hash=snapshot.source_hash,
        host=snapshot.host,
        port=snapshot.port,
        pid=snapshot.pid,
        fixture_path=snapshot.fixture_path,
        binary_path=snapshot.binary_path,
        message=snapshot.message,
    )

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
