from __future__ import annotations

from datetime import UTC, datetime
from collections.abc import Sequence
from types import SimpleNamespace
from typing import cast

import pytest
from app.services.verification_evidence import VerificationEvidenceRepository

from app.schemas.verification_schema import VerificationEvidenceDiagnosticSchema
from app.services.verification_run_evidence_service import load_verification_run_evidence


class _FakeEvidenceRepository:
    def __init__(self, evidence_rows: Sequence[object], evidence_set: object | None = None) -> None:
        self.evidence_rows: list[object] = list(evidence_rows)
        self.evidence_set: object | None = evidence_set
        self.list_calls: int = 0
        self.set_calls: int = 0

    async def list_signal_verification_evidence(
        self, *, workspace_id: int, test_run_id: str
    ) -> list[object]:
        _ = (workspace_id, test_run_id)
        self.list_calls += 1
        return list(self.evidence_rows)

    async def get_signal_verification_evidence_set(
        self, *, workspace_id: int, test_run_id: str
    ) -> object | None:
        _ = (workspace_id, test_run_id)
        self.set_calls += 1
        return self.evidence_set


def _repository(repo: _FakeEvidenceRepository) -> VerificationEvidenceRepository:
    return cast(VerificationEvidenceRepository, cast(object, repo))


@pytest.mark.anyio
async def test_load_verification_run_evidence_projects_steps_and_summary() -> None:
    evidence_rows = [
        SimpleNamespace(
            evidence_id="ev-1",
            signal_id=101,
            signal_path="signal-a",
            expected_path="expected-a",
            actual_report_path="actual-a",
            source_ied="IED-A",
            endpoint_id="sim:IED-A/P1",
            rpt_id="rpt-a",
            dataset="ds-a",
            observed_at=datetime(2026, 6, 23, 12, 0, tzinfo=UTC),
            latency_ms=35,
            quality="good",
            freshness="live",
            evidence_status="observed",
            reason_code="report_received",
            source_generation=5,
            source_report_sequence_generation=5,
            source_report_sequence_number=9,
            source_report_sub_sequence_number=None,
            report_reason="data-change",
            signal_value=True,
            timestamp_summary={"observed_at": "2026-06-23T12:00:00+00:00"},
            stale_reason=None,
            evidence_kind="report_observation",
            diagnostics=[VerificationEvidenceDiagnosticSchema(code="REPORT_RECEIVED", message="Received.")],
            created_at=datetime(2026, 6, 23, 12, 0, tzinfo=UTC),
        ),
        SimpleNamespace(
            evidence_id="ev-2",
            signal_id=202,
            signal_path="signal-b",
            expected_path="expected-b",
            actual_report_path=None,
            source_ied=None,
            endpoint_id="sim:IED-B/P1",
            rpt_id=None,
            dataset=None,
            observed_at=None,
            latency_ms=None,
            quality=None,
            freshness=None,
            evidence_status="timeout",
            reason_code="no_confirmation",
            source_generation=None,
            source_report_sequence_generation=None,
            source_report_sequence_number=None,
            source_report_sub_sequence_number=None,
            report_reason=None,
            signal_value=None,
            timestamp_summary={},
            stale_reason=None,
            evidence_kind="timeout",
            diagnostics=[VerificationEvidenceDiagnosticSchema(code="NO_CONFIRMATION", message="No confirmation.")],
            created_at=datetime(2026, 6, 23, 12, 1, tzinfo=UTC),
        ),
    ]
    evidence_set = SimpleNamespace(
        test_run_id="run-42",
        summary={"evidence_count": 2, "observed_count": 1, "timeout_count": 1},
        diagnostics=[{"code": "RUN_DIAGNOSTIC", "message": "run diagnostic"}],
    )
    repo = _FakeEvidenceRepository(evidence_rows, evidence_set=evidence_set)

    result = await load_verification_run_evidence(
        workspace_id=7,
        test_run_id="run-42",
        repository=_repository(repo),
    )

    assert repo.list_calls == 1
    assert repo.set_calls == 1
    assert result.test_run_id == "run-42"
    assert result.evidence_set.summary.evidence_count == 2
    assert result.evidence_set.summary.timeout_count == 1
    assert len(result.evidence_rows) == 2
    assert [step.signal_id for step in result.verification_steps] == [101, 202]
    assert [step.verdict_state for step in result.verification_steps] == ["pass", "fail"]
    assert [step.verification_confidence for step in result.verification_steps] == ["simulated", "degraded"]
    assert [step.confidence_reason for step in result.verification_steps] == [
        "simulator_generated_report",
        "degraded_recovery_state",
    ]
    assert result.verification_steps[0].source_session_id == "run-42:sim:IED-A/P1"
    assert result.verification_steps[1].source_session_id == "run-42:sim:IED-B/P1"
    assert result.verification_steps[0].session_id == "run-42:sim:IED-A/P1"
    assert result.verification_steps[0].subscription_id == "run-42:sim:IED-A/P1:rpt-a"
    assert result.verification_steps[1].subscription_id == "run-42:sim:IED-B/P1:subscription"
    assert result.verification_steps[0].source_report_rpt_id == "rpt-a"
    assert result.verification_steps[1].evidence_status == "timeout"
    assert result.diagnostics[0].code == "RUN_DIAGNOSTIC"
