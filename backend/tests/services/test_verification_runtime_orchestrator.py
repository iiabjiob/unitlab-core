from __future__ import annotations

from datetime import UTC, datetime

import pytest

from app.schemas.verification_schema import VerificationExecutionContextSchema
from app.services.iec61850.report_runtime import Iec61850DeviceEndpoint, Iec61850RuntimeMode
from app.services.verification_planner import VerificationTargetSource, build_verification_subscription_plan
from app.services.verification_runtime_orchestrator import VerificationRuntimeOrchestrator


def _build_multi_ied_plan():
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
            VerificationTargetSource(
                signal_id=202,
                signal_reference="Breaker Close B",
                signal_path="breaker_close_b",
                signal_metadata={
                    "protocol": "iec61850",
                    "protocol_metadata": {
                        "ied_name": "IED-B",
                        "access_point_name": "P1",
                        "report_control_reference_hint": "IED-B/P1/LLN0.brB/buffered",
                        "report_control_name": "brB",
                        "report_kind": "buffered",
                        "rpt_id": "IED-B/LLN0.brB",
                        "data_set_reference": "IED-B/LLN0.dsB",
                        "expected_feedback_path": "LD0/XCBR2.Pos.stVal[ST]",
                    },
                },
                allocation_id=2,
                allocation_status="assigned",
                allocation_health={
                    "conflict": False,
                    "invalid_type": False,
                    "missing_device": False,
                    "missing_channel": False,
                    "offline_device": False,
                    "stale_device": False,
                },
                channel_id=21,
                channel_label="DO-21",
                unit_id="IED-B/P1",
                unit_online=True,
                source_row_id="signal-202",
            ),
        ]
    )


def _custom_endpoint_for_device(device) -> Iec61850DeviceEndpoint:
    return Iec61850DeviceEndpoint(
        id=f"custom:{device.ied_name}/{device.access_point_name}",
        mode=Iec61850RuntimeMode.SIMULATOR,
        ied_name=device.ied_name,
        access_point_name=device.access_point_name,
        host=None,
        port=102,
    )


@pytest.mark.anyio
async def test_runtime_orchestrator_opens_sessions_and_keeps_live_state() -> None:
    plan = _build_multi_ied_plan()
    orchestrator = VerificationRuntimeOrchestrator(now=lambda: datetime(2026, 6, 23, 12, 0, tzinfo=UTC))

    result = orchestrator.start(
        workspace_id=7,
        test_run_id="run-1",
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

    assert result.verification_run.workflow_state == "running"
    assert result.verification_run.verdict_state == "pending"
    assert result.verification_run.runtime_state == "reporting"
    assert result.verification_run.evidence_set.summary.evidence_count == 0
    assert len(result.session_snapshots) == 2
    assert {snapshot.endpoint_id for snapshot in result.session_snapshots} == {
        "sim:IED-A/P1",
        "sim:IED-B/P1",
    }
    assert all(snapshot.runtime_state == "reporting" for snapshot in result.session_snapshots)
    assert len(result.subscription_snapshots) == 2
    assert {snapshot.endpoint_id for snapshot in result.subscription_snapshots} == {
        "sim:IED-A/P1",
        "sim:IED-B/P1",
    }
    assert all(snapshot.subscription_state == "reporting" for snapshot in result.subscription_snapshots)
    assert all(snapshot.report_health == "healthy" for snapshot in result.subscription_snapshots)
    assert all(snapshot.current_rptena_owner == "unitlab-backend-simulator" for snapshot in result.subscription_snapshots)
    assert result.verification_run.runtime_summary["active_sessions"] == 2
    assert result.verification_run.runtime_summary["reporting_sessions"] == 2
    assert result.verification_run.runtime_summary["active_subscriptions"] == 2
    assert result.verification_run.runtime_summary["reporting_subscriptions"] == 2

    snapshot = orchestrator.snapshot(result.orchestration_id)
    assert snapshot.verification_run.runtime_state == "reporting"
    assert snapshot.verification_run.verdict_state == "pending"
    assert len(snapshot.active_session_ids) == 2

    stopped = orchestrator.stop(result.orchestration_id)
    assert stopped.verification_run.runtime_state == "closed"
    assert stopped.verification_run.runtime_summary["closed_sessions"] == 2

    with pytest.raises(RuntimeError):
        orchestrator.snapshot(result.orchestration_id)


@pytest.mark.anyio
async def test_runtime_orchestrator_uses_injected_endpoint_for_device() -> None:
    plan = _build_multi_ied_plan()
    orchestrator = VerificationRuntimeOrchestrator(now=lambda: datetime(2026, 6, 23, 12, 0, tzinfo=UTC))

    result = orchestrator.start(
        workspace_id=7,
        test_run_id="run-custom-endpoint",
        verification_targets=plan.targets,
        subscription_plan=plan,
        execution_context=VerificationExecutionContextSchema(
            project_id=1,
            signal_list_revision_id=2,
            planner_version="test",
            runtime_version="simulator",
            policy_version="v1",
        ),
        endpoint_for_device=_custom_endpoint_for_device,
    )

    assert {snapshot.endpoint_id for snapshot in result.session_snapshots} == {
        "custom:IED-A/P1",
        "custom:IED-B/P1",
    }
    assert {snapshot.endpoint_id for snapshot in result.subscription_snapshots} == {
        "custom:IED-A/P1",
        "custom:IED-B/P1",
    }


@pytest.mark.anyio
async def test_runtime_orchestrator_reconnects_one_session_without_affecting_the_other() -> None:
    plan = _build_multi_ied_plan()
    orchestrator = VerificationRuntimeOrchestrator(now=lambda: datetime(2026, 6, 23, 12, 0, tzinfo=UTC))

    result = orchestrator.start(
        workspace_id=7,
        test_run_id="run-2",
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

    session_ids = [snapshot.session_id for snapshot in result.session_snapshots]
    reconnected = orchestrator.reconnect(result.orchestration_id, session_ids[0])

    session_snapshots = {snapshot.session_id: snapshot for snapshot in reconnected.session_snapshots}
    subscription_snapshots = {snapshot.session_id: snapshot for snapshot in reconnected.subscription_snapshots}

    assert reconnected.verification_run.runtime_state == "reporting"
    assert reconnected.verification_run.recovery_state is None
    assert session_snapshots[session_ids[0]].connection_generation == 2
    assert session_snapshots[session_ids[0]].runtime_state == "reporting"
    assert session_snapshots[session_ids[1]].connection_generation == 1
    assert session_snapshots[session_ids[1]].runtime_state == "reporting"
    assert subscription_snapshots[session_ids[0]].subscription_state == "reporting"
    assert subscription_snapshots[session_ids[1]].subscription_state == "reporting"


@pytest.mark.anyio
async def test_runtime_orchestrator_reconnect_failure_surfaces_recovery_state(monkeypatch: pytest.MonkeyPatch) -> None:
    plan = _build_multi_ied_plan()
    orchestrator = VerificationRuntimeOrchestrator(now=lambda: datetime(2026, 6, 23, 12, 0, tzinfo=UTC))

    result = orchestrator.start(
        workspace_id=7,
        test_run_id="run-3",
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

    handle = orchestrator._handles[result.orchestration_id]  # noqa: SLF001
    session_id = result.session_snapshots[0].session_id

    def _raise_open_session(**kwargs):  # noqa: ANN001
        raise RuntimeError("simulated reconnect failure")

    monkeypatch.setattr(handle.runtime_service, "open_session", _raise_open_session)

    recovered = orchestrator.reconnect(result.orchestration_id, session_id)
    session_snapshots = {snapshot.session_id: snapshot for snapshot in recovered.session_snapshots}

    assert recovered.verification_run.runtime_state == "degraded"
    assert recovered.verification_run.recovery_state is not None
    assert recovered.verification_run.recovery_state.recovery_reason == "runtime_failure"
    assert recovered.verification_run.recovery_state.desired_state == "reconnecting"
    assert session_snapshots[session_id].runtime_state == "failed"
    assert session_snapshots[session_id].connection_generation == 1
    assert session_snapshots[result.session_snapshots[1].session_id].runtime_state == "reporting"
