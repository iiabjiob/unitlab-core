from __future__ import annotations

from typing import Any

from fastapi import APIRouter, HTTPException
from pydantic import BaseModel

from app.services.core_diag_service import (
    CoreDiagCommandAccepted,
    enqueue_core_diag_command,
    get_core_diag_state,
)

router = APIRouter(prefix="/api/v1/core-diagnostics", tags=["Core Diagnostics"])


class CoreDiagCommandAcceptedResponse(BaseModel):
    request_id: str
    action: str
    queued_at: str


class CoreDiagStateResponse(BaseModel):
    state: dict[str, Any]


def _accepted_to_response(accepted: CoreDiagCommandAccepted) -> CoreDiagCommandAcceptedResponse:
    return CoreDiagCommandAcceptedResponse(
        request_id=accepted.request_id,
        action=accepted.action,
        queued_at=accepted.queued_at.isoformat(),
    )


@router.get("/state", response_model=CoreDiagStateResponse)
async def core_diag_state() -> CoreDiagStateResponse:
    state = await get_core_diag_state()
    if state is None:
        raise HTTPException(status_code=503, detail="Core diagnostics state unavailable")
    return CoreDiagStateResponse(state=state)


@router.post("/status", response_model=CoreDiagCommandAcceptedResponse)
async def request_core_diag_status() -> CoreDiagCommandAcceptedResponse:
    accepted = await enqueue_core_diag_command("status")
    return _accepted_to_response(accepted)

