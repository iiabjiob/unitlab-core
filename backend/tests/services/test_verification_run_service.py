from __future__ import annotations

from datetime import UTC, datetime
from types import SimpleNamespace

import pytest

from app.schemas.verification_schema import (
    SignalVerificationEvidenceSchema,
    SignalVerificationEvidenceSetSchema,
    SignalVerificationEvidenceSetSummarySchema,
    VerificationAutoRunStartSchema,
    VerificationExecutionContextSchema,
    VerificationRunDetailResponseSchema,
    VerificationRunSchema,
    VerificationSubscriptionPlanCoverageSchema,
    VerificationSubscriptionPlanSchema,
    VerificationStepSchema,
    VerificationTargetSchema,
    VerificationVerdictExplanationSchema,
)
from app.services import verification_run_service as run_service
from app.services.verification_run_service import (
    execute_single_signal_verification_run,
    load_verification_run_detail,
)
from app.services.verification_verdict_explanation_service import build_verification_verdict_explanation


def _build_verification_run(signal_reference: str = "Breaker Close") -> VerificationRunSchema:
    evidence = SignalVerificationEvidenceSchema(
        evidence_id="ev-1",
        signal_id=101,
        signal_path="breaker_close",
        expected_path="LD0/XCBR1.Pos.stVal",
        actual_report_path="LD0/XCBR1.Pos.stVal",
        source_ied="IED-A",
        endpoint_id="IED-A/P1",
        rpt_id="rpt-a",
        dataset="ds-a",
        observed_at=datetime(2026, 6, 23, 12, 0, 0, 250_000, tzinfo=UTC),
        latency_ms=250,
        quality="good",
        freshness="live",
        evidence_status="observed",
        reason_code="report_received",
        diagnostics=[],
    )
    return VerificationRunSchema(
        test_run_id="run-1",
        verification_targets=[
            VerificationTargetSchema(
                signal_id=101,
                signal_reference=signal_reference,
                signal_path="breaker_close",
                endpoint_id="IED-A/P1",
                expected_feedback_path="LD0/XCBR1.Pos.stVal",
                timeout_ms=5000,
                window_ms=1000,
                protocol="iec61850",
                protocol_metadata={},
                coverage_state="exact",
                coverage_reason=None,
                allocation_id=1,
                channel_id=11,
                channel_label="DO-11",
                unit_id="IED-A/P1",
                source_row_id="signal-101",
            )
        ],
        subscription_plan=VerificationSubscriptionPlanSchema(
            plan_id="plan-1",
            selected_signal_ids=[101],
            targets=[
                VerificationTargetSchema(
                    signal_id=101,
                    signal_reference=signal_reference,
                    signal_path="breaker_close",
                    endpoint_id="IED-A/P1",
                    expected_feedback_path="LD0/XCBR1.Pos.stVal",
                    timeout_ms=5000,
                    window_ms=1000,
                    protocol="iec61850",
                    protocol_metadata={},
                    coverage_state="exact",
                    coverage_reason=None,
                    allocation_id=1,
                    channel_id=11,
                    channel_label="DO-11",
                    unit_id="IED-A/P1",
                    source_row_id="signal-101",
                )
            ],
            groups=[],
            uncovered_targets=[],
            planning_diagnostics=[],
            coverage=VerificationSubscriptionPlanCoverageSchema(),
        ),
        session_snapshots=[],
        evidence_set=SignalVerificationEvidenceSetSchema(
            test_run_id="run-1",
            evidence=[evidence],
            summary=SignalVerificationEvidenceSetSummarySchema(
                evidence_count=1,
                observed_count=1,
                source_generation=None,
            ),
            diagnostics=[],
        ),
        execution_context=VerificationExecutionContextSchema(
            project_id=1,
            signal_list_revision_id=2,
            planner_version="test",
            runtime_version="simulator",
            policy_version="v1",
        ),
        workflow_state="completed",
        verdict_state="pass",
        selected_group_id=None,
        operator_id=None,
        triggered_at=datetime(2026, 6, 23, 12, 0, tzinfo=UTC),
        completed_at=datetime(2026, 6, 23, 12, 0, 0, 250_000, tzinfo=UTC),
        runtime_state="reporting",
        runtime_summary={"active_sessions": 1},
        reason=None,
        diagnostics=[],
        verification_steps=[
            VerificationStepSchema(
                step_id="step-101",
                signal_id=101,
                target_index=0,
                step_state="completed",
                expected_path="LD0/XCBR1.Pos.stVal",
                expected_window_ms=1000,
                freshness="live",
                evidence_status="observed",
                verdict_state="pass",
                evidence_ids=["ev-1"],
                actual_report_path="LD0/XCBR1.Pos.stVal",
                source_session_id="IED-A/P1",
                source_report_rpt_id="rpt-a",
                source_report_dat_set="ds-a",
                triggered_at=datetime(2026, 6, 23, 12, 0, tzinfo=UTC),
                observed_at=datetime(2026, 6, 23, 12, 0, 0, 250_000, tzinfo=UTC),
                latency_ms=250,
                reason="report_received",
                diagnostics=[],
            )
        ],
    )


def test_build_verification_verdict_explanation_includes_signal_context_and_summary() -> None:
    verification_run = _build_verification_run()
    evidence_rows = [
        SignalVerificationEvidenceSchema(
            evidence_id="ev-1",
            signal_id=101,
            signal_path="breaker_close",
            expected_path="LD0/XCBR1.Pos.stVal",
            actual_report_path="LD0/XCBR1.Pos.stVal",
            source_ied="IED-A",
            endpoint_id="IED-A/P1",
            rpt_id="rpt-a",
            dataset="ds-a",
            observed_at=datetime(2026, 6, 23, 12, 0, 0, 250_000, tzinfo=UTC),
            latency_ms=250,
            quality="good",
            freshness="live",
            evidence_status="observed",
            reason_code="report_received",
            diagnostics=[],
        )
    ]

    explanation = build_verification_verdict_explanation(
        verification_run=verification_run,  # type: ignore[arg-type]
        verification_steps=verification_run.verification_steps,
        evidence_rows=evidence_rows,
    )

    assert explanation.verdict_state == "pass"
    assert explanation.headline == "PASS"
    assert "observed" in explanation.summary.lower()
    assert explanation.signals[0].signal_reference == "Breaker Close"
    assert explanation.signals[0].observed_path == "LD0/XCBR1.Pos.stVal"
    assert explanation.signals[0].latency_ms == 250


def test_build_verification_verdict_explanation_marks_timeout_fail() -> None:
    verification_run = _build_verification_run()
    verification_run.verdict_state = "fail"
    verification_run.verification_steps[0] = VerificationStepSchema(
        step_id="step-101",
        signal_id=101,
        target_index=0,
        step_state="failed",
        expected_path="LD0/XCBR1.Pos.stVal",
        expected_window_ms=1000,
        freshness="unknown",
        evidence_status="timeout",
        verdict_state="fail",
        evidence_ids=["ev-1"],
        diagnostics=[],
    )
    evidence_rows = [
        SignalVerificationEvidenceSchema(
            evidence_id="ev-1",
            signal_id=101,
            signal_path="breaker_close",
            expected_path="LD0/XCBR1.Pos.stVal",
            actual_report_path=None,
            source_ied=None,
            endpoint_id="IED-A/P1",
            rpt_id=None,
            dataset=None,
            observed_at=None,
            latency_ms=None,
            quality=None,
            freshness=None,
            evidence_status="timeout",
            reason_code="no_confirmation",
            diagnostics=[],
        )
    ]

    explanation = build_verification_verdict_explanation(
        verification_run=verification_run,  # type: ignore[arg-type]
        verification_steps=verification_run.verification_steps,
        evidence_rows=evidence_rows,
    )

    assert explanation.verdict_state == "fail"
    assert explanation.headline == "FAIL"
    assert "timeout" in explanation.summary.lower()
    assert explanation.signals[0].evidence_status == "timeout"


class _FakeDb:
    def __init__(self) -> None:
        self.flushed = 0
        self.committed = 0
        self.rolled_back = 0

    async def flush(self) -> None:
        self.flushed += 1

    async def commit(self) -> None:
        self.committed += 1

    async def rollback(self) -> None:
        self.rolled_back += 1


class _FakeSignalsRepository:
    def __init__(self, db: _FakeDb) -> None:
        self.db = db

    async def ensure_workspace(self, workspace_id: int) -> bool:
        return workspace_id == 7

    async def list_by_ids(self, workspace_id: int, signal_ids):
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
                        "report_control_reference_hint": "IED-A/P1/LLN0.brA/buffered",
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
        self.db = db

    async def list_allocation_rows_by_signal_ids(self, workspace_id: int, signal_ids):
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


class _FakeEvidenceRepository:
    def __init__(self, db: _FakeDb) -> None:
        self.db = db
        self.rows = []
        self.evidence_sets = []

    async def record_signal_verification_evidence(self, **kwargs):
        self.rows.append(dict(kwargs))
        return SimpleNamespace(**kwargs)

    async def upsert_signal_verification_evidence_set(self, **kwargs):
        self.evidence_sets.append(dict(kwargs))
        return SimpleNamespace(**kwargs)


class _FakeRunRepository:
    def __init__(self, db: _FakeDb) -> None:
        self.db = db
        self.rows = {}

    async def upsert_signal_verification_run(self, *, workspace_id: int, test_run_id: str, payload: dict):
        self.rows[(workspace_id, test_run_id)] = dict(payload)
        return SimpleNamespace(workspace_id=workspace_id, test_run_id=test_run_id, payload=dict(payload))

    async def get_signal_verification_run(self, *, workspace_id: int, test_run_id: str):
        payload = self.rows.get((workspace_id, test_run_id))
        if payload is None:
            return None
        return SimpleNamespace(workspace_id=workspace_id, test_run_id=test_run_id, payload=dict(payload))


@pytest.mark.anyio
async def test_execute_single_signal_verification_run_persists_run_snapshot_and_explanation(monkeypatch) -> None:
    db = _FakeDb()
    triggered_at = datetime(2026, 6, 23, 12, 0, tzinfo=UTC)

    monkeypatch.setattr(run_service, "SignalsRepository", _FakeSignalsRepository)
    monkeypatch.setattr(run_service, "SignalSheetRepository", _FakeSignalSheetRepository)
    monkeypatch.setattr(run_service, "VerificationEvidenceRepository", _FakeEvidenceRepository)
    monkeypatch.setattr(run_service, "VerificationRunRepository", _FakeRunRepository)

    result = await execute_single_signal_verification_run(
        workspace_id=7,
        payload=VerificationAutoRunStartSchema(
            signal_ids=[101],
            execution_context=VerificationExecutionContextSchema(
                project_id=1,
                signal_list_revision_id=2,
                planner_version="test",
                runtime_version="simulator",
                policy_version="v1",
            ),
            client_id="unitlab-backend-simulator",
        ),
        db=db,  # type: ignore[arg-type]
        triggered_at=triggered_at,
    )

    assert result.verification_run.workflow_state == "completed"
    assert result.verification_run.verdict_state == "pass"
    assert result.verdict_explanation.headline == "PASS"
    assert result.verdict_explanation.signals[0].observed_path == "LD0/XCBR1.Pos.stVal"
    assert result.verification_run.reason == result.verdict_explanation.summary
    assert result.as_response().test_run_id == result.test_run_id
    assert db.flushed >= 1


@pytest.mark.anyio
async def test_load_verification_run_detail_returns_persisted_payload(monkeypatch) -> None:
    db = _FakeDb()
    monkeypatch.setattr(run_service, "VerificationRunRepository", _FakeRunRepository)

    payload = VerificationRunDetailResponseSchema(
        test_run_id="run-1",
        verification_run=_build_verification_run(),
        verdict_explanation=VerificationVerdictExplanationSchema(
            test_run_id="run-1",
            verdict_state="pass",
            headline="PASS",
            summary="PASS: observed LD0/XCBR1.Pos.stVal on IED-A/P1 in 250 ms.",
            signals=[],
            diagnostics=[],
        ),
    ).model_dump(mode="json")
    repository = _FakeRunRepository(db)
    repository.rows[(7, "run-1")] = payload
    monkeypatch.setattr(run_service, "VerificationRunRepository", lambda _db: repository)

    result = await load_verification_run_detail(
        workspace_id=7,
        test_run_id="run-1",
        db=db,  # type: ignore[arg-type]
    )

    assert result.test_run_id == "run-1"
    assert result.verdict_explanation.headline == "PASS"
    assert result.verification_run.verdict_state == "pass"
