from __future__ import annotations

from fastapi import APIRouter, File, Form, HTTPException, UploadFile
from fastapi.encoders import jsonable_encoder

from app.services.iec61850.client_control import get_iec61850_client_control_service
from app.services.iec61850.report_runtime import Iec61850ReportRuntimeError
from app.services.iec61850.scl_import import import_scl_source, scl_import_result_to_payload

router = APIRouter(prefix="/api/v1/iec61850/client", tags=["IEC 61850 Client"])


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


@router.post("/scl/import")
async def import_scl_model(
    file: UploadFile = File(...),
    selected_ied_name: str | None = Form(default=None),
) -> dict:
    contents = await file.read()
    try:
        xml_text = contents.decode("utf-8")
    except UnicodeDecodeError as exc:
        raise HTTPException(
            status_code=400,
            detail={"code": "SCL_ENCODING_UNSUPPORTED", "message": "SCL import expects UTF-8 XML."},
        ) from exc

    result = import_scl_source(
        file_name=file.filename or "uploaded.scd",
        xml_text=xml_text,
        selected_ied_name=selected_ied_name,
    )
    return jsonable_encoder(scl_import_result_to_payload(result))


def _run_action(action: str, operation) -> dict:
    try:
        snapshot = operation()
    except Iec61850ReportRuntimeError as exc:
        raise HTTPException(
            status_code=409,
            detail={"action": action, "code": exc.code, "message": str(exc)},
        ) from exc
    return jsonable_encoder(snapshot)
