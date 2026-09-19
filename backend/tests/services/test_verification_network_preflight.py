from __future__ import annotations

from types import SimpleNamespace
from typing import cast

import pytest
from sqlalchemy.ext.asyncio import AsyncSession

from app.schemas.verification_schema import (
    VerificationAutoRunStartSchema,
    VerificationExecutionContextSchema,
)
from app.services import verification_network_preflight as preflight_service
from app.services.verification_network_preflight import build_verification_network_preflight_response


class _FakeDb:
    async def execute(self, _stmt: object) -> SimpleNamespace:
        _ = _stmt
        return SimpleNamespace(first=lambda: None)


class _FakeSignalsRepository:
    def __init__(self, db: _FakeDb) -> None:
        self.db: _FakeDb = db

    async def ensure_workspace(self, workspace_id: int) -> bool:
        return workspace_id == 7

    async def list_by_ids(self, workspace_id: int, signal_ids: list[int]) -> list[SimpleNamespace]:
        _ = signal_ids
        if workspace_id != 7:
            return []
        return [
            SimpleNamespace(
                id=101,
                key="breaker_close",
                name="Breaker Close",
                signal_metadata={
                    "protocol": "iec61850",
                    "protocol_metadata": {
                        "ied_name": "IED-A",
                        "access_point_name": "P1",
                        "transport_host": "10.10.10.250",
                        "report_control_name": "brA",
                        "report_kind": "buffered",
                        "rpt_id": "IED-A/LLN0.brA",
                        "data_set_reference": "IED-A/LLN0.dsA",
                        "expected_feedback_path": "LD0/XCBR1.Pos.stVal",
                    },
                },
            )
        ]


class _FakeSignalSheetRepository:
    def __init__(self, db: _FakeDb) -> None:
        self.db: _FakeDb = db

    async def list_allocation_rows_by_signal_ids(
        self, workspace_id: int, signal_ids: list[int]
    ) -> list[SimpleNamespace]:
        _ = signal_ids
        if workspace_id != 7:
            return []
        return [
            SimpleNamespace(
                row_id="signal-101",
                signal_id=101,
                allocation_id=1,
                allocation_status="assigned",
                allocation_health={},
                channel_id=11,
                channel_label="DO-11",
                unit_id="IED-A/P1",
                unit_online=True,
            )
        ]


class _FakeRuntimeSelectionRepository:
    def __init__(self, _db: _FakeDb) -> None:
        _ = _db
        pass

    async def get_active_runtime_selection(self, *, workspace_id: int) -> None:
        _ = workspace_id
        return None

    async def get_import_source(self, *, workspace_id: int, import_id: str) -> None:
        _ = (workspace_id, import_id)
        return None


@pytest.mark.anyio
async def test_build_verification_network_preflight_response_uses_agent_network_state_for_ready_mms(monkeypatch: pytest.MonkeyPatch) -> None:
    db = _FakeDb()

    async def _get_core_network_state():
        return {
            "status": "connected",
            "interfaces": [
                {
                    "interface_name": "eth0",
                    "local_ip": "10.10.10.20",
                    "netmask": "255.255.255.0",
                    "network": "10.10.10.0/24",
                }
            ],
        }

    monkeypatch.setattr(preflight_service, "SignalsRepository", _FakeSignalsRepository)
    monkeypatch.setattr(preflight_service, "SignalSheetRepository", _FakeSignalSheetRepository)
    monkeypatch.setattr(preflight_service, "Iec61850SqlAlchemySclImportRepository", _FakeRuntimeSelectionRepository)
    monkeypatch.setattr(preflight_service, "get_core_network_state", _get_core_network_state)

    result = await build_verification_network_preflight_response(
        workspace_id=7,
        payload=VerificationAutoRunStartSchema(
            signal_ids=[101],
            execution_context=VerificationExecutionContextSchema(
                project_id=1,
                signal_list_revision_id=2,
                planner_version="test",
                runtime_version="mms",
                policy_version="v1",
            ),
            client_id="unitlab-backend-simulator",
        ),
        db=cast(AsyncSession, cast(object, db)),
    )

    preflight = result.preflight
    assert preflight.overall_state == "ready"
    assert preflight.recommended_runtime_version == "mms"
    assert preflight.groups[0].readiness_state == "ready"
    assert preflight.groups[0].recommended_interface_name == "eth0"
    assert any(diagnostic.code == "network_preflight_core_state_observed" for diagnostic in preflight.diagnostics)
    assert any(diagnostic.code == "network_preflight_signal_catalog" for diagnostic in preflight.diagnostics)


@pytest.mark.anyio
async def test_build_verification_network_preflight_response_falls_back_to_simulator_when_core_state_missing(monkeypatch: pytest.MonkeyPatch) -> None:
    db = _FakeDb()

    async def _get_core_network_state():
        return None

    monkeypatch.setattr(preflight_service, "SignalsRepository", _FakeSignalsRepository)
    monkeypatch.setattr(preflight_service, "SignalSheetRepository", _FakeSignalSheetRepository)
    monkeypatch.setattr(preflight_service, "Iec61850SqlAlchemySclImportRepository", _FakeRuntimeSelectionRepository)
    monkeypatch.setattr(preflight_service, "get_core_network_state", _get_core_network_state)

    result = await build_verification_network_preflight_response(
        workspace_id=7,
        payload=VerificationAutoRunStartSchema(
            signal_ids=[101],
            execution_context=VerificationExecutionContextSchema(
                project_id=1,
                signal_list_revision_id=2,
                planner_version="test",
                runtime_version="mms",
                policy_version="v1",
            ),
            client_id="unitlab-backend-simulator",
        ),
        db=cast(AsyncSession, cast(object, db)),
    )

    preflight = result.preflight
    assert preflight.overall_state == "unknown"
    assert preflight.recommended_runtime_version == "simulator"
    assert any(diagnostic.code == "network_preflight_core_state_unavailable" for diagnostic in preflight.diagnostics)
