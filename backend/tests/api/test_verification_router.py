from __future__ import annotations

from datetime import UTC, datetime
from typing import cast

import pytest
from sqlalchemy.ext.asyncio import AsyncSession

from app.api.v1.verification import router as verification_router
from app.schemas.verification_schema import (
    VerificationAutoRunStartSchema,
    VerificationExecutionContextSchema,
    VerificationRuntimeOrchestrationReconnectSchema,
)
from app.services.verification_planner import VerificationTargetSource, build_verification_subscription_plan
from app.services.verification_run_service import VerificationRuntimeStartContext
from app.services.verification_runtime_orchestrator import VerificationRuntimeOrchestrator
from app.services.verification_runtime_selection import resolve_verification_runtime


def _build_single_ied_plan():
    return build_verification_subscription_plan(
        [
            VerificationTargetSource(
                signal_id=101,
                signal_reference="Breaker Close A",
                signal_path="breaker_close_a",
                signal_metadata={
                    "protocol": "iec61850",
                    "protocol_metadata": {
                        "ied_name": "IED-A",
                        "access_point_name": "P1",
                        "report_control_reference_hint": "IED-A/P1/LLN0.brA/buffered",
                        "report_control_name": "brA",
                        "report_kind": "buffered",
                        "rpt_id": "IED-A/LLN0.brA",
                        "data_set_reference": "IED-A/LLN0.dsA",
                        "expected_feedback_path": "LD0/XCBR1.Pos.stVal[ST]",
                    },
                },
                allocation_id=1,
                allocation_status="assigned",
                allocation_health={
                    "conflict": False,
                    "invalid_type": False,
                    "missing_device": False,
                    "missing_channel": False,
                    "offline_device": False,
                    "stale_device": False,
                },
                channel_id=11,
                channel_label="DO-11",
                unit_id="IED-A/P1",
                unit_online=True,
                source_row_id="signal-101",
            ),
        ]
    )


@pytest.mark.anyio
async def test_reconnect_route_returns_updated_runtime_snapshot(monkeypatch: pytest.MonkeyPatch) -> None:
    plan = _build_single_ied_plan()
    orchestrator = VerificationRuntimeOrchestrator(now=lambda: datetime(2026, 6, 23, 12, 0, tzinfo=UTC))
    result = orchestrator.start(
        workspace_id=7,
        test_run_id="run-route",
        verification_targets=plan.targets,
        subscription_plan=plan,
        execution_context=VerificationExecutionContextSchema(
            project_id=1,
            signal_list_revision_id=2,
            planner_version="test",
            runtime_version="simulator",
            policy_version="v1",
        ),
    )
    monkeypatch.setattr(verification_router, "_orchestrator", orchestrator)

    response = await verification_router.reconnect_verification_runtime_orchestration(
        workspace_id=7,
        orchestration_id=result.orchestration_id,
        payload=VerificationRuntimeOrchestrationReconnectSchema(session_id=result.session_snapshots[0].session_id),
    )

    assert response.orchestration_id == result.orchestration_id
    assert response.verification_run.runtime_state == "reporting"
    assert response.verification_run.recovery_state is None
    assert response.verification_run.session_snapshots[0].connection_generation == 2


@pytest.mark.anyio
async def test_start_orchestration_from_signals_uses_backend_runtime_context(monkeypatch: pytest.MonkeyPatch) -> None:
    plan = _build_single_ied_plan()
    orchestrator = VerificationRuntimeOrchestrator(now=lambda: datetime(2026, 6, 23, 12, 0, tzinfo=UTC))
    execution_context = VerificationExecutionContextSchema(
        project_id=1,
        signal_list_revision_id=2,
        planner_version="test",
        runtime_version="simulator",
        policy_version="v1",
    )
    runtime_selection = resolve_verification_runtime(
        execution_context=execution_context,
        now=lambda: datetime(2026, 6, 23, 12, 0, tzinfo=UTC),
    )

    async def _build_context(**kwargs: object) -> VerificationRuntimeStartContext:
        del kwargs
        return VerificationRuntimeStartContext(
            selected_signal_ids=[101],
            subscription_plan=plan,
            execution_context=execution_context,
            runtime_selection=runtime_selection,
            diagnostics=(),
        )

    monkeypatch.setattr(verification_router, "_orchestrator", orchestrator)

    def _noop_deferred_startup(_orchestration_id: str) -> None:
        return None

    monkeypatch.setattr(
        orchestrator,
        "_run_deferred_startup",
        _noop_deferred_startup,
    )
    monkeypatch.setattr(
        verification_router, "build_verification_runtime_start_context", _build_context
    )

    response = await verification_router.start_verification_runtime_orchestration_from_signals(
        workspace_id=7,
        payload=VerificationAutoRunStartSchema(
            signal_ids=[101],
            test_run_id="online-route",
            client_id="unitlab-online-61850",
            execution_context=execution_context,
        ),
        db=cast(AsyncSession, cast(object, None)),
    )

    assert response.orchestration_id.startswith("7:online-route:")
    assert response.verification_run.runtime_state == "connecting"
    assert response.verification_run.subscription_snapshots[0].subscription_state == "pending"
