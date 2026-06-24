from __future__ import annotations

from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy.ext.asyncio import AsyncSession

from app.infrastructure.db.database import get_db
from app.schemas.verification_schema import (
    VerificationAutoRunStartSchema,
    VerificationNetworkPreflightResponseSchema,
    VerificationRunDetailResponseSchema,
    VerificationRunEvidenceResponseSchema,
    VerificationRuntimeOrchestrationResponseSchema,
    VerificationRuntimeOrchestrationReconnectSchema,
    VerificationRuntimeOrchestrationStartSchema,
    VerificationRunStepDetailsSchema,
)
from app.services.verification_evidence import VerificationEvidenceRepository
from app.services.verification_network_preflight import build_verification_network_preflight_response
from app.services.verification_run_service import (
    execute_single_signal_verification_run,
    load_verification_run_detail,
)
from app.services.verification_run_evidence_service import load_verification_run_evidence
from app.services.verification_runtime_orchestrator import VerificationRuntimeOrchestrator

router = APIRouter(prefix="/api/v1/workspaces/{workspace_id}/verification", tags=["Verification"])

_orchestrator = VerificationRuntimeOrchestrator()


def get_verification_evidence_repo(db: AsyncSession = Depends(get_db)) -> VerificationEvidenceRepository:
    return VerificationEvidenceRepository(db)


@router.post("/preflight", response_model=VerificationNetworkPreflightResponseSchema)
async def preflight_verification_run(
    workspace_id: int,
    payload: VerificationAutoRunStartSchema,
    db: AsyncSession = Depends(get_db),
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


@router.post("/runs", response_model=VerificationRunDetailResponseSchema)
async def start_verification_run(
    workspace_id: int,
    payload: VerificationAutoRunStartSchema,
    db: AsyncSession = Depends(get_db),
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
    db: AsyncSession = Depends(get_db),
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
        result = _orchestrator.start(
            workspace_id=workspace_id,
            test_run_id=payload.test_run_id,
            verification_targets=payload.verification_targets,
            subscription_plan=payload.subscription_plan,
            execution_context=payload.execution_context,
            client_id=payload.client_id,
        )
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
    repo: VerificationEvidenceRepository = Depends(get_verification_evidence_repo),
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
    repo: VerificationEvidenceRepository = Depends(get_verification_evidence_repo),
):
    result = await load_verification_run_evidence(
        workspace_id=workspace_id,
        test_run_id=test_run_id,
        repository=repo,
    )
    return result.as_step_response()
