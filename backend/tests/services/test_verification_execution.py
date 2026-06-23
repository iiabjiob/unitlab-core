from __future__ import annotations

from datetime import UTC, datetime, timedelta
from types import SimpleNamespace

import pytest

from app.schemas.verification_schema import VerificationExecutionContextSchema
from app.services.verification_execution import execute_simulated_verification_run
from app.services.verification_planner import VerificationTargetSource, build_verification_subscription_plan


class _FakeVerificationEvidenceRepository:
    def __init__(self) -> None:
        self.rows: list[dict] = []
        self.evidence_sets: list[dict] = []

    async def record_signal_verification_evidence(self, **kwargs):
        self.rows.append(dict(kwargs))
        return SimpleNamespace(**kwargs)

    async def upsert_signal_verification_evidence_set(self, **kwargs):
        self.evidence_sets.append(dict(kwargs))
        return SimpleNamespace(**kwargs)


def _build_plan():
    return build_verification_subscription_plan(
        [
            VerificationTargetSource(
                signal_id=101,
                signal_reference="Breaker Close",
                signal_path="breaker_close",
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
            )
        ]
    )


@pytest.mark.anyio
async def test_execute_simulated_verification_run_records_observed_evidence_and_passes() -> None:
    plan = _build_plan()
    repo = _FakeVerificationEvidenceRepository()
    triggered_at = datetime(2026, 6, 23, 12, 0, tzinfo=UTC)

    result = await execute_simulated_verification_run(
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
        repository=repo,  # type: ignore[arg-type]
        triggered_at=triggered_at,
        latency_ms=250,
        now=lambda: triggered_at + timedelta(milliseconds=250),
    )

    assert result.verification_run.workflow_state == "completed"
    assert result.verification_run.verdict_state == "pass"
    assert result.evidence_set.summary.evidence_count == 1
    assert result.evidence_set.summary.observed_count == 1
    assert result.verification_run.verification_steps[0].evidence_status == "observed"
    assert result.verification_run.verification_steps[0].verdict_state == "pass"
    assert repo.rows[0]["evidence_status"] == "observed"
    assert repo.evidence_sets[0]["evidence"][0].signal_id == 101


@pytest.mark.anyio
async def test_execute_simulated_verification_run_marks_late_observation_as_fail() -> None:
    plan = _build_plan()
    plan.targets[0].window_ms = 500
    plan.targets[0].timeout_ms = 2000
    repo = _FakeVerificationEvidenceRepository()
    triggered_at = datetime(2026, 6, 23, 12, 0, tzinfo=UTC)

    result = await execute_simulated_verification_run(
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
        repository=repo,  # type: ignore[arg-type]
        triggered_at=triggered_at,
        latency_ms=1500,
        now=lambda: triggered_at + timedelta(milliseconds=1500),
    )

    assert result.verification_run.workflow_state == "completed"
    assert result.verification_run.verdict_state == "fail"
    assert result.evidence_set.summary.late_count == 1
    assert result.verification_run.verification_steps[0].evidence_status == "late"
    assert result.verification_run.verification_steps[0].verdict_state == "fail"
    assert repo.rows[0]["evidence_status"] == "late"
