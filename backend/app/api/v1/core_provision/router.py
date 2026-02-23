from __future__ import annotations

from typing import Any

from fastapi import APIRouter, HTTPException
from pydantic import BaseModel

from app.services.core_provision_service import (
    CoreProvisionCommandAccepted,
    enqueue_core_provision_command,
    get_core_provision_state,
)

router = APIRouter(prefix="/api/v1/core-provision", tags=["Core Provisioning"])


class CoreProvisionCommandAcceptedResponse(BaseModel):
    request_id: str
    action: str
    queued_at: str


class CoreProvisionStateResponse(BaseModel):
    state: dict[str, Any]


def _accepted_to_response(accepted: CoreProvisionCommandAccepted) -> CoreProvisionCommandAcceptedResponse:
    return CoreProvisionCommandAcceptedResponse(
        request_id=accepted.request_id,
        action=accepted.action,
        queued_at=accepted.queued_at.isoformat(),
    )


@router.get("/state", response_model=CoreProvisionStateResponse)
async def core_provision_state() -> CoreProvisionStateResponse:
    state = await get_core_provision_state()
    if state is None:
        raise HTTPException(status_code=503, detail="Core provisioning state unavailable")
    return CoreProvisionStateResponse(state=state)


@router.post("/status", response_model=CoreProvisionCommandAcceptedResponse)
async def core_provision_status() -> CoreProvisionCommandAcceptedResponse:
    return _accepted_to_response(await enqueue_core_provision_command("status"))


@router.post("/smoke-check", response_model=CoreProvisionCommandAcceptedResponse)
async def core_provision_smoke_check() -> CoreProvisionCommandAcceptedResponse:
    return _accepted_to_response(await enqueue_core_provision_command("smoke_check"))


@router.post("/install/net-agent", response_model=CoreProvisionCommandAcceptedResponse)
async def core_provision_install_net_agent() -> CoreProvisionCommandAcceptedResponse:
    return _accepted_to_response(await enqueue_core_provision_command("install_net_agent"))


@router.post("/install/ntp-agent", response_model=CoreProvisionCommandAcceptedResponse)
async def core_provision_install_ntp_agent() -> CoreProvisionCommandAcceptedResponse:
    return _accepted_to_response(await enqueue_core_provision_command("install_ntp_agent"))


@router.post("/install/diag-agent", response_model=CoreProvisionCommandAcceptedResponse)
async def core_provision_install_diag_agent() -> CoreProvisionCommandAcceptedResponse:
    return _accepted_to_response(await enqueue_core_provision_command("install_diag_agent"))

