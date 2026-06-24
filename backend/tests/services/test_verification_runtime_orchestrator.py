from __future__ import annotations

from datetime import UTC, datetime

import pytest

from app.schemas.verification_schema import VerificationExecutionContextSchema
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
