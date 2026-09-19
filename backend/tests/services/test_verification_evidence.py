from __future__ import annotations

from datetime import UTC, datetime
from types import SimpleNamespace
from typing import cast

import pytest
from sqlalchemy.ext.asyncio import AsyncSession

from app.schemas.verification_schema import (
    SignalVerificationEvidenceSchema,
    VerificationEvidenceDiagnosticSchema,
)
from app.services.verification_evidence import (
    VerificationEvidenceRepository,
    build_signal_verification_evidence_set,
    build_signal_verification_evidence_summary,
)


def test_build_signal_verification_evidence_summary_counts_statuses() -> None:
    evidence = [
        SignalVerificationEvidenceSchema(
            evidence_id="ev-1",
            signal_id=10,
            signal_path="signal-a",
            expected_path="expected-a",
            actual_report_path="expected-a",
            source_ied="IED-A",
            endpoint_id="endpoint-a",
            rpt_id="rpt-a",
            dataset="dataset-a",
            observed_at=datetime(2026, 6, 23, 12, 0, tzinfo=UTC),
            latency_ms=12,
            quality="good",
            freshness="live",
            evidence_status="observed",
            reason_code="report_received",
            source_generation=7,
        ),
        SignalVerificationEvidenceSchema(
            evidence_id="ev-2",
            signal_id=10,
            signal_path="signal-a",
            expected_path="expected-a",
            actual_report_path="expected-a",
            source_ied="IED-A",
            endpoint_id="endpoint-a",
            rpt_id="rpt-a",
            dataset="dataset-a",
            observed_at=datetime(2026, 6, 23, 12, 0, tzinfo=UTC),
            latency_ms=155,
            quality="good",
            freshness="stale",
            evidence_status="late",
            reason_code="window_exceeded",
            source_generation=7,
        ),
        SignalVerificationEvidenceSchema(
            evidence_id="ev-3",
            signal_id=10,
            signal_path="signal-a",
            expected_path="expected-a",
            actual_report_path="expected-a",
            source_ied="IED-A",
            endpoint_id="endpoint-a",
            rpt_id="rpt-a",
            dataset="dataset-a",
            observed_at=datetime(2026, 6, 23, 12, 0, tzinfo=UTC),
            latency_ms=999,
            quality="bad",
            freshness="unknown",
            evidence_status="timeout",
            reason_code="no_confirmation",
            source_generation=7,
        ),
    ]

    summary = build_signal_verification_evidence_summary(evidence)

    assert summary.evidence_count == 3
    assert summary.observed_count == 1
    assert summary.stale_count == 0
    assert summary.timeout_count == 1
    assert summary.invalid_count == 0
    assert summary.late_count == 1
    assert summary.out_of_window_count == 0
    assert summary.source_generation == 7


def test_build_signal_verification_evidence_set_preserves_diagnostics() -> None:
    evidence = [
        SignalVerificationEvidenceSchema(
            evidence_id="ev-10",
            signal_id=11,
            signal_path="signal-b",
            expected_path="expected-b",
            actual_report_path="expected-b",
            source_ied="IED-B",
            endpoint_id="endpoint-b",
            rpt_id="rpt-b",
            dataset="dataset-b",
            observed_at=datetime(2026, 6, 23, 12, 0, tzinfo=UTC),
            latency_ms=31,
            quality="good",
            freshness="live",
            evidence_status="observed",
            reason_code="report_received",
        )
    ]

    evidence_set = build_signal_verification_evidence_set(
        test_run_id="run-17",
        evidence=evidence,
        diagnostics=[VerificationEvidenceDiagnosticSchema(code="REPORT_RECEIVED", message="Report received.")],
    )

    assert evidence_set.test_run_id == "run-17"
    assert evidence_set.summary.evidence_count == 1
    assert evidence_set.diagnostics[0].code == "REPORT_RECEIVED"


class _FakeExecuteResult:
    def __init__(self, value: object | None = None) -> None:
        self.value: object | None = value

    def scalar_one_or_none(self) -> object | None:
        return self.value


class _FakeListExecuteResult(_FakeExecuteResult):
    def __init__(self, values: list[object]) -> None:
        super().__init__()
        self.values: list[object] = list(values)

    def scalars(self) -> _FakeListExecuteResult:
        return self

    def all(self) -> list[object]:
        return list(self.values)


class _FakeAsyncSession:
    def __init__(self, existing: object | None = None) -> None:
        self.existing: object | None = existing
        self.added: list[object] = []
        self.flushed: int = 0
        self.execute_count: int = 0

    async def execute(self, _stmt: object) -> _FakeExecuteResult:
        _ = _stmt
        self.execute_count += 1
        return _FakeExecuteResult(self.existing)

    def add(self, row: object) -> None:
        self.added.append(row)

    async def flush(self) -> None:
        self.flushed += 1


def _repository(session: object) -> VerificationEvidenceRepository:
    return VerificationEvidenceRepository(cast(AsyncSession, session))


@pytest.mark.anyio
async def test_verification_evidence_repository_records_append_only_rows() -> None:
    session = _FakeAsyncSession()
    repository = _repository(session)

    first = await repository.record_signal_verification_evidence(
        workspace_id=9,
        test_run_id="run-7",
        signal_list_revision_id=42,
        evidence_id="ev-1",
        signal_id=100,
        signal_path="signal-x",
        expected_path="expected-x",
        actual_report_path="actual-x",
        source_ied="IED-X",
        endpoint_id="endpoint-x",
        rpt_id="rpt-x",
        dataset="dataset-x",
        observed_at=datetime(2026, 6, 23, 12, 0, tzinfo=UTC),
        latency_ms=44,
        quality="good",
        freshness="live",
        evidence_status="observed",
        reason_code="report_received",
        diagnostics=[VerificationEvidenceDiagnosticSchema(code="REPORT_RECEIVED", message="Report received.")],
    )
    second = await repository.record_signal_verification_evidence(
        workspace_id=9,
        test_run_id="run-7",
        evidence_id="ev-2",
        signal_id=100,
        signal_path="signal-x",
        expected_path="expected-x",
        actual_report_path="actual-x",
        source_ied="IED-X",
        endpoint_id="endpoint-x",
        rpt_id="rpt-x",
        dataset="dataset-x",
        observed_at=datetime(2026, 6, 23, 12, 0, tzinfo=UTC),
        latency_ms=211,
        quality="good",
        freshness="stale",
        evidence_status="late",
        reason_code="window_exceeded",
    )

    assert len(session.added) == 2
    assert first.evidence_id == "ev-1"
    assert first.signal_list_revision_id == 42
    assert first.diagnostics[0]["code"] == "REPORT_RECEIVED"
    assert second.evidence_id == "ev-2"
    assert second.evidence_status == "late"
    assert session.flushed == 2


@pytest.mark.anyio
async def test_verification_evidence_repository_updates_run_summary_without_touching_rows() -> None:
    existing = SimpleNamespace(summary={"evidence_count": 1}, diagnostics=[{"code": "OLD", "message": "old"}])
    session = _FakeAsyncSession(existing=existing)
    repository = _repository(session)

    evidence = [
        SignalVerificationEvidenceSchema(
            evidence_id="ev-11",
            signal_id=11,
            signal_path="signal-y",
            expected_path="expected-y",
            actual_report_path="actual-y",
            source_ied="IED-Y",
            endpoint_id="endpoint-y",
            rpt_id="rpt-y",
            dataset="dataset-y",
            observed_at=datetime(2026, 6, 23, 12, 0, tzinfo=UTC),
            latency_ms=33,
            quality="good",
            freshness="live",
            evidence_status="observed",
            reason_code="report_received",
        )
    ]

    evidence_set = await repository.upsert_signal_verification_evidence_set(
        workspace_id=9,
        test_run_id="run-8",
        evidence=evidence,
        diagnostics=[VerificationEvidenceDiagnosticSchema(code="RUN_UPDATED", message="Updated summary.")],
    )

    assert evidence_set is existing
    assert evidence_set.summary["evidence_count"] == 1
    assert evidence_set.diagnostics[0]["code"] == "RUN_UPDATED"
    assert session.added == []
    assert session.flushed == 1


@pytest.mark.anyio
async def test_verification_evidence_repository_lists_rows_for_inspection() -> None:
    rows = [
        SimpleNamespace(evidence_id="ev-2", created_at=datetime(2026, 6, 23, 12, 0, tzinfo=UTC)),
        SimpleNamespace(evidence_id="ev-3", created_at=datetime(2026, 6, 23, 12, 1, tzinfo=UTC)),
    ]

    class _ListingSession(_FakeAsyncSession):
        execute_count: int

        async def execute(self, _stmt: object) -> _FakeListExecuteResult:  # pyright: ignore[reportImplicitOverride]
            _ = _stmt
            self.execute_count += 1
            return _FakeListExecuteResult(cast(list[object], rows))

    repository = _repository(_ListingSession())

    result = await repository.list_signal_verification_evidence(workspace_id=9, test_run_id="run-9")

    assert [row.evidence_id for row in result] == ["ev-2", "ev-3"]


@pytest.mark.anyio
async def test_verification_evidence_repository_allows_timeout_rows_with_nullable_fields() -> None:
    session = _FakeAsyncSession()
    repository = _repository(session)

    evidence = await repository.record_signal_verification_evidence(
        workspace_id=9,
        test_run_id="run-10",
        evidence_id="ev-timeout",
        signal_id=101,
        signal_path="signal-timeout",
        expected_path="expected-timeout",
        observed_at=None,
        actual_report_path=None,
        source_ied=None,
        endpoint_id="endpoint-timeout",
        rpt_id=None,
        dataset=None,
        latency_ms=None,
        quality=None,
        freshness=None,
        evidence_status="timeout",
        reason_code="no_confirmation",
    )

    assert len(session.added) == 1
    assert evidence.actual_report_path is None
    assert evidence.observed_at is None
    assert evidence.latency_ms is None
    assert evidence.quality is None
    assert evidence.freshness is None
    assert evidence.source_ied is None
