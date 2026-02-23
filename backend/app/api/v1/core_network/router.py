from __future__ import annotations

from typing import Any

from fastapi import APIRouter, HTTPException
from pydantic import BaseModel

from app.services.core_network_service import (
    CoreNetworkCommandAccepted,
    CoreNetworkCommandScanPayload,
    CoreNetworkConnectPayload,
    enqueue_core_network_command,
    get_core_network_state,
)

router = APIRouter(prefix="/api/v1/core-network", tags=["Core Network"])


class CoreNetworkCommandAcceptedResponse(BaseModel):
    request_id: str
    action: str
    queued_at: str


class CoreNetworkStateResponse(BaseModel):
    state: dict[str, Any]


def _accepted_to_response(accepted: CoreNetworkCommandAccepted) -> CoreNetworkCommandAcceptedResponse:
    return CoreNetworkCommandAcceptedResponse(
        request_id=accepted.request_id,
        action=accepted.action,
        queued_at=accepted.queued_at.isoformat(),
    )


@router.get("/state", response_model=CoreNetworkStateResponse)
async def core_network_state() -> CoreNetworkStateResponse:
    state = await get_core_network_state()
    if state is None:
        raise HTTPException(status_code=503, detail="Core network state unavailable")
    return CoreNetworkStateResponse(state=state)


@router.post("/status", response_model=CoreNetworkCommandAcceptedResponse)
async def request_core_network_status() -> CoreNetworkCommandAcceptedResponse:
    accepted = await enqueue_core_network_command("status")
    return _accepted_to_response(accepted)


@router.post("/scan", response_model=CoreNetworkCommandAcceptedResponse)
async def scan_core_networks(payload: CoreNetworkCommandScanPayload | None = None) -> CoreNetworkCommandAcceptedResponse:
    body: dict[str, Any] = {}
    if payload and payload.timeout_sec is not None:
        body["timeout_sec"] = payload.timeout_sec
    accepted = await enqueue_core_network_command("scan", payload=body)
    return _accepted_to_response(accepted)


@router.post("/connect", response_model=CoreNetworkCommandAcceptedResponse)
async def connect_core_network_sta(payload: CoreNetworkConnectPayload) -> CoreNetworkCommandAcceptedResponse:
    accepted = await enqueue_core_network_command(
        "connect_sta",
        payload=payload.model_dump(exclude_none=True),
    )
    return _accepted_to_response(accepted)


@router.post("/disconnect", response_model=CoreNetworkCommandAcceptedResponse)
async def disconnect_core_network_sta() -> CoreNetworkCommandAcceptedResponse:
    accepted = await enqueue_core_network_command("disconnect_sta")
    return _accepted_to_response(accepted)


@router.post("/restart-ap", response_model=CoreNetworkCommandAcceptedResponse)
async def restart_core_network_ap() -> CoreNetworkCommandAcceptedResponse:
    accepted = await enqueue_core_network_command("restart_ap")
    return _accepted_to_response(accepted)

