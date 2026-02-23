from __future__ import annotations

from typing import Any

from fastapi import APIRouter, HTTPException
from pydantic import BaseModel

from app.services.core_ntp_service import (
    CoreNtpApplyServersPayload,
    CoreNtpCommandAccepted,
    enqueue_core_ntp_command,
    get_core_ntp_state,
)

router = APIRouter(prefix="/api/v1/core-ntp", tags=["Core NTP"])


class CoreNtpCommandAcceptedResponse(BaseModel):
    request_id: str
    action: str
    queued_at: str


class CoreNtpStateResponse(BaseModel):
    state: dict[str, Any]


def _accepted_to_response(accepted: CoreNtpCommandAccepted) -> CoreNtpCommandAcceptedResponse:
    return CoreNtpCommandAcceptedResponse(
        request_id=accepted.request_id,
        action=accepted.action,
        queued_at=accepted.queued_at.isoformat(),
    )


@router.get("/state", response_model=CoreNtpStateResponse)
async def core_ntp_state() -> CoreNtpStateResponse:
    state = await get_core_ntp_state()
    if state is None:
        raise HTTPException(status_code=503, detail="Core NTP state unavailable")
    return CoreNtpStateResponse(state=state)


@router.post("/status", response_model=CoreNtpCommandAcceptedResponse)
async def request_core_ntp_status() -> CoreNtpCommandAcceptedResponse:
    accepted = await enqueue_core_ntp_command("status")
    return _accepted_to_response(accepted)


@router.put("/servers", response_model=CoreNtpCommandAcceptedResponse)
async def apply_core_ntp_servers(payload: CoreNtpApplyServersPayload) -> CoreNtpCommandAcceptedResponse:
    accepted = await enqueue_core_ntp_command("apply_servers", payload={"servers": payload.servers})
    return _accepted_to_response(accepted)


@router.post("/restore-defaults", response_model=CoreNtpCommandAcceptedResponse)
async def restore_core_ntp_defaults() -> CoreNtpCommandAcceptedResponse:
    accepted = await enqueue_core_ntp_command("restore_defaults")
    return _accepted_to_response(accepted)


@router.post("/reload", response_model=CoreNtpCommandAcceptedResponse)
async def reload_core_ntp_sources() -> CoreNtpCommandAcceptedResponse:
    accepted = await enqueue_core_ntp_command("reload")
    return _accepted_to_response(accepted)

