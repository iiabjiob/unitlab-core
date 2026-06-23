from __future__ import annotations

from fastapi import APIRouter, HTTPException

from app.schemas.verification_schema import (
    VerificationRuntimeOrchestrationResponseSchema,
    VerificationRuntimeOrchestrationStartSchema,
)
from app.services.verification_runtime_orchestrator import VerificationRuntimeOrchestrator

router = APIRouter(prefix="/api/v1/workspaces/{workspace_id}/verification", tags=["Verification"])

_orchestrator = VerificationRuntimeOrchestrator()


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
