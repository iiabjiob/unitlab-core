from __future__ import annotations

from typing import Annotated

import asyncio

from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy.ext.asyncio import AsyncSession

from app.infrastructure.db.database import get_db
from app.schemas.verification_schema import (
    VerificationAutoRunStartSchema,
    VerificationExternalIedTargetsRequestSchema,
    VerificationExternalIedDiscoveryTreeResponseSchema,
    VerificationExternalIedManualReportRequestSchema,
    VerificationExternalIedManualReportResponseSchema,
    VerificationMmsReachabilityRequestSchema,
    VerificationMmsReachabilityResponseSchema,
    VerificationNetworkPreflightResponseSchema,
    VerificationRunDetailResponseSchema,
    VerificationRunEvidenceResponseSchema,
    VerificationRuntimeOrchestrationResponseSchema,
    VerificationRuntimeOrchestrationReconnectSchema,
    VerificationRuntimeOrchestrationStartSchema,
    VerificationRunStepDetailsSchema,
)
from app.services.external_ied_availability import (
    configure_external_ied_targets,
    load_external_ied_discovery_tree,
    publish_external_ied_status_snapshot,
)
from app.services.external_ied_manual_control import (
    ExternalIedManualReportRequest,
    get_external_ied_manual_report_control_service,
)
from app.services.external_ied_discovery_scheduler import schedule_external_ied_discovery_for_user_request
from app.services.verification_evidence import VerificationEvidenceRepository
from app.services.verification_network_preflight import build_verification_network_preflight_response
from app.services.verification_mms_reachability import check_mms_tcp_reachability
from app.services.verification_run_service import (
    build_verification_runtime_start_context,
    execute_single_signal_verification_run,
    load_verification_run_detail,
)
from app.services.verification_run_evidence_service import load_verification_run_evidence
from app.services.verification_endpoint_resolution import (
    build_verification_endpoint_resolution_diagnostic,
    resolve_verification_endpoint_resolution_policy,
)
from app.services.verification_runtime_selection import resolve_verification_runtime
from app.services.verification_signal_endpoint_catalog import build_verification_plan_endpoint_catalog
from app.services.verification_runtime_orchestrator import VerificationRuntimeOrchestrator

router = APIRouter(prefix="/api/v1/workspaces/{workspace_id}/verification", tags=["Verification"])

_orchestrator = VerificationRuntimeOrchestrator()


def get_verification_evidence_repo(db: Annotated[AsyncSession, Depends(get_db)]) -> VerificationEvidenceRepository:
    return VerificationEvidenceRepository(db)


@router.post("/preflight", response_model=VerificationNetworkPreflightResponseSchema)
async def preflight_verification_run(
    workspace_id: int,
    payload: VerificationAutoRunStartSchema,
    db: Annotated[AsyncSession, Depends(get_db)],
):
    try:
        result = await build_verification_network_preflight_response(
            workspace_id=workspace_id,
            payload=payload,
            db=db,
        )
    except ValueError as exc:
        status_code = 404 if str(exc) == "Workspace not found" else 400
        raise HTTPException(status_code=status_code, detail=str(exc)) from exc
    except Exception as exc:  # noqa: BLE001
        raise HTTPException(status_code=500, detail=str(exc)) from exc
    return result


@router.post("/mms-reachability", response_model=VerificationMmsReachabilityResponseSchema)
async def probe_mms_reachability(
    workspace_id: int,
    payload: VerificationMmsReachabilityRequestSchema,
):
    _ = workspace_id
    return await check_mms_tcp_reachability(payload)


@router.put("/external-ieds/targets")
async def set_external_ied_targets(
    workspace_id: int,
    payload: VerificationExternalIedTargetsRequestSchema,
):
    return await configure_external_ied_targets(
        workspace_id,
        [target.model_dump(mode="json") for target in payload.targets],
    )


@router.post("/external-ieds/{endpoint}/discovery/refresh")
async def refresh_external_ied_discovery(
    workspace_id: int,
    endpoint: str,
):
    try:
        request = await schedule_external_ied_discovery_for_user_request(
            workspace_id=workspace_id,
            endpoint=endpoint,
        )
    except ValueError as exc:
        raise HTTPException(status_code=409, detail=str(exc)) from exc
    except Exception as exc:  # noqa: BLE001
        raise HTTPException(status_code=500, detail=str(exc)) from exc
    if request is None:
        raise HTTPException(status_code=404, detail="External IED endpoint is not reachable or not configured")
    _ = await publish_external_ied_status_snapshot(workspace_id)
    return {"queued": True, "request_id": request.request_id, "endpoint": request.endpoint}


@router.get("/external-ieds/{endpoint}/discovery/tree", response_model=VerificationExternalIedDiscoveryTreeResponseSchema)
async def get_external_ied_discovery_tree(
    workspace_id: int,
    endpoint: str,
):
    tree = await load_external_ied_discovery_tree(workspace_id=workspace_id, endpoint=endpoint)
    if tree is None:
        raise HTTPException(status_code=404, detail="External IED discovery model is not available")
    return tree


@router.post("/external-ieds/{endpoint}/reports/enable", response_model=VerificationExternalIedManualReportResponseSchema)
async def enable_external_ied_report(
    workspace_id: int,
    endpoint: str,
    payload: VerificationExternalIedManualReportRequestSchema,
):
    try:
        return await asyncio.to_thread(
            get_external_ied_manual_report_control_service().set_report_enabled,
            ExternalIedManualReportRequest(
                workspace_id=workspace_id,
                endpoint=endpoint,
                report_reference=payload.report_reference,
                report_name=payload.report_name,
                report_kind=payload.report_kind,
                dataset_reference=payload.dataset_reference,
            ),
            enabled=True,
        )
    except ValueError as exc:
        raise HTTPException(status_code=400, detail=str(exc)) from exc
    except Exception as exc:  # noqa: BLE001
        raise HTTPException(status_code=500, detail=str(exc)) from exc


@router.post("/external-ieds/{endpoint}/reports/disable", response_model=VerificationExternalIedManualReportResponseSchema)
async def disable_external_ied_report(
    workspace_id: int,
    endpoint: str,
    payload: VerificationExternalIedManualReportRequestSchema,
):
    try:
        return await asyncio.to_thread(
            get_external_ied_manual_report_control_service().set_report_enabled,
            ExternalIedManualReportRequest(
                workspace_id=workspace_id,
                endpoint=endpoint,
                report_reference=payload.report_reference,
                report_name=payload.report_name,
                report_kind=payload.report_kind,
                dataset_reference=payload.dataset_reference,
            ),
            enabled=False,
        )
    except ValueError as exc:
        raise HTTPException(status_code=400, detail=str(exc)) from exc
    except Exception as exc:  # noqa: BLE001
        raise HTTPException(status_code=500, detail=str(exc)) from exc


@router.post("/external-ieds/{endpoint}/reports/leases/{lease_id}/heartbeat", response_model=VerificationExternalIedManualReportResponseSchema)
async def heartbeat_external_ied_report_lease(
    workspace_id: int,
    endpoint: str,
    lease_id: str,
):
    try:
        return await asyncio.to_thread(
            get_external_ied_manual_report_control_service().renew_lease,
            workspace_id=workspace_id,
            endpoint=endpoint,
            lease_id=lease_id,
        )
    except ValueError as exc:
        raise HTTPException(status_code=404, detail=str(exc)) from exc
    except Exception as exc:  # noqa: BLE001
        raise HTTPException(status_code=500, detail=str(exc)) from exc


@router.post("/external-ieds/{endpoint}/reports/leases/{lease_id}/gi", response_model=VerificationExternalIedManualReportResponseSchema)
async def gi_external_ied_report_lease(
    workspace_id: int,
    endpoint: str,
    lease_id: str,
):
    try:
        return await asyncio.to_thread(
            get_external_ied_manual_report_control_service().send_general_interrogation,
            workspace_id=workspace_id,
            endpoint=endpoint,
            lease_id=lease_id,
        )
    except ValueError as exc:
        raise HTTPException(status_code=404, detail=str(exc)) from exc
    except Exception as exc:  # noqa: BLE001
        raise HTTPException(status_code=500, detail=str(exc)) from exc


@router.post("/external-ieds/{endpoint}/reports/leases/{lease_id}/release", response_model=VerificationExternalIedManualReportResponseSchema)
async def release_external_ied_report_lease(
    workspace_id: int,
    endpoint: str,
    lease_id: str,
):
    try:
        return await asyncio.to_thread(
            get_external_ied_manual_report_control_service().release_lease,
            workspace_id=workspace_id,
            endpoint=endpoint,
            lease_id=lease_id,
        )
    except ValueError as exc:
        raise HTTPException(status_code=404, detail=str(exc)) from exc
    except Exception as exc:  # noqa: BLE001
        raise HTTPException(status_code=500, detail=str(exc)) from exc


@router.post("/runs", response_model=VerificationRunDetailResponseSchema)
async def start_verification_run(
    workspace_id: int,
    payload: VerificationAutoRunStartSchema,
    db: Annotated[AsyncSession, Depends(get_db)],
):
    try:
        result = await execute_single_signal_verification_run(
            workspace_id=workspace_id,
            payload=payload,
            db=db,
        )
        await db.commit()
    except ValueError as exc:
        await db.rollback()
        status_code = 404 if str(exc) == "Workspace not found" else 400
        raise HTTPException(status_code=status_code, detail=str(exc)) from exc
    except Exception as exc:  # noqa: BLE001
        await db.rollback()
        raise HTTPException(status_code=500, detail=str(exc)) from exc
    return result.as_response()


@router.get("/runs/{test_run_id}", response_model=VerificationRunDetailResponseSchema)
async def get_verification_run(
    workspace_id: int,
    test_run_id: str,
    db: Annotated[AsyncSession, Depends(get_db)],
):
    try:
        return await load_verification_run_detail(
            workspace_id=workspace_id,
            test_run_id=test_run_id,
            db=db,
        )
    except ValueError as exc:
        raise HTTPException(status_code=404, detail=str(exc)) from exc


@router.post("/orchestrations", response_model=VerificationRuntimeOrchestrationResponseSchema)
async def start_verification_runtime_orchestration(
    workspace_id: int,
    payload: VerificationRuntimeOrchestrationStartSchema,
):
    try:
        plan_endpoint_catalog = build_verification_plan_endpoint_catalog(payload.subscription_plan)
        endpoint_resolution_policy = resolve_verification_endpoint_resolution_policy(
            execution_context=payload.execution_context,
            explicit_mms_endpoint_catalog=plan_endpoint_catalog,
            transport_override_host=payload.execution_context.transport_override_host,
            transport_override_port=payload.execution_context.transport_override_port,
        )
        runtime_selection = resolve_verification_runtime(
            execution_context=payload.execution_context,
            endpoint_catalog=endpoint_resolution_policy.endpoint_catalog,
            transport_source=endpoint_resolution_policy.transport_source,
            model_source=endpoint_resolution_policy.model_source,
            transport_override_host=endpoint_resolution_policy.transport_override_host,
            transport_override_port=endpoint_resolution_policy.transport_override_port,
        )
        result = _orchestrator.start(
            workspace_id=workspace_id,
            test_run_id=payload.test_run_id,
            verification_targets=payload.verification_targets,
            subscription_plan=payload.subscription_plan,
            execution_context=payload.execution_context,
            client_id=payload.client_id,
            endpoint_for_device=runtime_selection.endpoint_for_device,
            adapter=runtime_selection.adapter,
            initial_diagnostics=(build_verification_endpoint_resolution_diagnostic(endpoint_resolution_policy),),
        )
    except ValueError as exc:
        raise HTTPException(status_code=400, detail=str(exc)) from exc
    except RuntimeError as exc:
        raise HTTPException(status_code=409, detail=str(exc)) from exc
    return VerificationRuntimeOrchestrationResponseSchema(
        orchestration_id=result.orchestration_id,
        verification_run=result.verification_run,
    )


@router.post("/orchestrations/from-signals", response_model=VerificationRuntimeOrchestrationResponseSchema)
async def start_verification_runtime_orchestration_from_signals(
    workspace_id: int,
    payload: VerificationAutoRunStartSchema,
    db: Annotated[AsyncSession, Depends(get_db)],
):
    try:
        context = await build_verification_runtime_start_context(
            workspace_id=workspace_id,
            payload=payload,
            db=db,
        )
        result = _orchestrator.start_deferred(
            workspace_id=workspace_id,
            test_run_id=str(payload.test_run_id or f"online-61850-{workspace_id}"),
            verification_targets=context.subscription_plan.targets,
            subscription_plan=context.subscription_plan,
            execution_context=context.execution_context,
            client_id=payload.client_id or "unitlab-online-61850",
            endpoint_for_device=context.runtime_selection.endpoint_for_device,
            adapter=context.runtime_selection.adapter,
            initial_diagnostics=context.diagnostics,
        )
    except ValueError as exc:
        status_code = 404 if str(exc) == "Workspace not found" else 400
        raise HTTPException(status_code=status_code, detail=str(exc)) from exc
    except RuntimeError as exc:
        raise HTTPException(status_code=409, detail=str(exc)) from exc
    return VerificationRuntimeOrchestrationResponseSchema(
        orchestration_id=result.orchestration_id,
        verification_run=result.verification_run,
    )


@router.get("/orchestrations/{orchestration_id}", response_model=VerificationRuntimeOrchestrationResponseSchema)
async def get_verification_runtime_orchestration(
    workspace_id: int,
    orchestration_id: str,
):
    try:
        result = _orchestrator.snapshot(orchestration_id)
    except RuntimeError as exc:
        raise HTTPException(status_code=404, detail=str(exc)) from exc
    if not result.orchestration_id.startswith(f"{workspace_id}:"):
        raise HTTPException(status_code=404, detail="Orchestration not found")
    return VerificationRuntimeOrchestrationResponseSchema(
        orchestration_id=result.orchestration_id,
        verification_run=result.verification_run,
    )


@router.post("/orchestrations/{orchestration_id}/stop", response_model=VerificationRuntimeOrchestrationResponseSchema)
async def stop_verification_runtime_orchestration(
    workspace_id: int,
    orchestration_id: str,
):
    try:
        result = _orchestrator.stop(orchestration_id)
    except RuntimeError as exc:
        raise HTTPException(status_code=404, detail=str(exc)) from exc
    if not result.orchestration_id.startswith(f"{workspace_id}:"):
        raise HTTPException(status_code=404, detail="Orchestration not found")
    return VerificationRuntimeOrchestrationResponseSchema(
        orchestration_id=result.orchestration_id,
        verification_run=result.verification_run,
    )


@router.post("/orchestrations/{orchestration_id}/reconnect", response_model=VerificationRuntimeOrchestrationResponseSchema)
async def reconnect_verification_runtime_orchestration(
    workspace_id: int,
    orchestration_id: str,
    payload: VerificationRuntimeOrchestrationReconnectSchema,
):
    try:
        result = _orchestrator.reconnect(orchestration_id, payload.session_id, workspace_id=workspace_id)
    except RuntimeError as exc:
        raise HTTPException(status_code=404, detail=str(exc)) from exc
    return VerificationRuntimeOrchestrationResponseSchema(
        orchestration_id=result.orchestration_id,
        verification_run=result.verification_run,
    )


@router.get("/runs/{test_run_id}/evidence", response_model=VerificationRunEvidenceResponseSchema)
async def get_verification_run_evidence(
    workspace_id: int,
    test_run_id: str,
    repo: Annotated[VerificationEvidenceRepository, Depends(get_verification_evidence_repo)],
):
    result = await load_verification_run_evidence(
        workspace_id=workspace_id,
        test_run_id=test_run_id,
        repository=repo,
    )
    return result.as_response()


@router.get("/runs/{test_run_id}/steps", response_model=VerificationRunStepDetailsSchema)
async def get_verification_run_steps(
    workspace_id: int,
    test_run_id: str,
    repo: Annotated[VerificationEvidenceRepository, Depends(get_verification_evidence_repo)],
):
    result = await load_verification_run_evidence(
        workspace_id=workspace_id,
        test_run_id=test_run_id,
        repository=repo,
    )
    return result.as_step_response()
