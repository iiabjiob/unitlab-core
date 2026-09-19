from __future__ import annotations

from collections.abc import Sequence
from datetime import datetime
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession

from app.models.verification_evidence import SignalVerificationEvidence, SignalVerificationEvidenceSet
from app.schemas.verification_schema import (
    SignalVerificationEvidenceSchema,
    SignalVerificationEvidenceSetSchema,
    SignalVerificationEvidenceSetSummarySchema,
    VerificationEvidenceDiagnosticSchema,
)


def build_signal_verification_evidence_summary(
    evidence: Sequence[SignalVerificationEvidenceSchema],
) -> SignalVerificationEvidenceSetSummarySchema:
    statuses = [item.evidence_status for item in evidence]
    source_generations = {item.source_generation for item in evidence if item.source_generation is not None}
    return SignalVerificationEvidenceSetSummarySchema(
        evidence_count=len(evidence),
        observed_count=statuses.count("observed"),
        stale_count=statuses.count("stale"),
        timeout_count=statuses.count("timeout"),
        invalid_count=statuses.count("invalid"),
        late_count=statuses.count("late"),
        out_of_window_count=statuses.count("out_of_window"),
        source_generation=next(iter(source_generations)) if len(source_generations) == 1 else None,
    )


def build_signal_verification_evidence_set(
    *,
    test_run_id: str,
    evidence: Sequence[SignalVerificationEvidenceSchema],
    diagnostics: Sequence[VerificationEvidenceDiagnosticSchema] = (),
) -> SignalVerificationEvidenceSetSchema:
    return SignalVerificationEvidenceSetSchema(
        test_run_id=str(test_run_id),
        evidence=list(evidence),
        summary=build_signal_verification_evidence_summary(evidence),
        diagnostics=list(diagnostics),
    )


class VerificationEvidenceRepository:
    def __init__(self, db: AsyncSession):
        self.db: AsyncSession = db

    async def record_signal_verification_evidence(
        self,
        *,
        workspace_id: int,
        test_run_id: str,
        signal_list_revision_id: int | None = None,
        evidence_id: str,
        signal_id: int,
        signal_path: str,
        expected_path: str,
        actual_report_path: str | None = None,
        source_ied: str | None = None,
        endpoint_id: str | None = None,
        rpt_id: str | None = None,
        dataset: str | None = None,
        observed_at: datetime | None = None,
        latency_ms: int | None = None,
        quality: str | None = None,
        freshness: str | None = None,
        evidence_status: str,
        reason_code: str,
        source_generation: int | None = None,
        source_report_sequence_generation: int | None = None,
        source_report_sequence_number: int | None = None,
        source_report_sub_sequence_number: int | None = None,
        report_reason: str | None = None,
        signal_value: object | None = None,
        timestamp_summary: dict[str, object] | None = None,
        stale_reason: str | None = None,
        evidence_kind: str | None = None,
        diagnostics: Sequence[VerificationEvidenceDiagnosticSchema] = (),
    ) -> SignalVerificationEvidence:
        evidence = SignalVerificationEvidence(
            workspace_id=int(workspace_id),
            test_run_id=str(test_run_id),
            signal_list_revision_id=(
                int(signal_list_revision_id) if signal_list_revision_id is not None else None
            ),
            evidence_id=str(evidence_id),
            signal_id=int(signal_id),
            signal_path=str(signal_path),
            expected_path=str(expected_path),
            actual_report_path=str(actual_report_path) if actual_report_path is not None else None,
            source_ied=str(source_ied) if source_ied is not None else None,
            endpoint_id=str(endpoint_id) if endpoint_id is not None else None,
            rpt_id=str(rpt_id) if rpt_id is not None else None,
            dataset=str(dataset) if dataset is not None else None,
            observed_at=observed_at,
            latency_ms=int(latency_ms) if latency_ms is not None else None,
            quality=str(quality) if quality is not None else None,
            freshness=str(freshness) if freshness is not None else None,
            evidence_status=str(evidence_status),
            reason_code=str(reason_code),
            source_generation=int(source_generation) if source_generation is not None else None,
            source_report_sequence_generation=int(source_report_sequence_generation)
            if source_report_sequence_generation is not None
            else None,
            source_report_sequence_number=int(source_report_sequence_number)
            if source_report_sequence_number is not None
            else None,
            source_report_sub_sequence_number=int(source_report_sub_sequence_number)
            if source_report_sub_sequence_number is not None
            else None,
            report_reason=str(report_reason).strip() if report_reason is not None else None,
            signal_value=signal_value,
            timestamp_summary=dict(timestamp_summary or {}),
            stale_reason=str(stale_reason).strip() if stale_reason is not None else None,
            evidence_kind=str(evidence_kind).strip() if evidence_kind is not None else None,
            diagnostics=[item.model_dump() for item in diagnostics],
        )
        self.db.add(evidence)
        await self.db.flush()
        return evidence

    async def get_signal_verification_evidence_set(
        self,
        *,
        workspace_id: int,
        test_run_id: str,
    ) -> SignalVerificationEvidenceSet | None:
        stmt = select(SignalVerificationEvidenceSet).where(
            SignalVerificationEvidenceSet.workspace_id == workspace_id,
            SignalVerificationEvidenceSet.test_run_id == test_run_id,
        )
        result = await self.db.execute(stmt.limit(1))
        return result.scalar_one_or_none()

    async def list_signal_verification_evidence(
        self,
        *,
        workspace_id: int,
        test_run_id: str,
    ) -> list[SignalVerificationEvidence]:
        stmt = (
            select(SignalVerificationEvidence)
            .where(
                SignalVerificationEvidence.workspace_id == workspace_id,
                SignalVerificationEvidence.test_run_id == test_run_id,
            )
            .order_by(SignalVerificationEvidence.created_at.asc(), SignalVerificationEvidence.id.asc())
        )
        result = await self.db.execute(stmt)
        return list(result.scalars().all())

    async def upsert_signal_verification_evidence_set(
        self,
        *,
        workspace_id: int,
        test_run_id: str,
        evidence: Sequence[SignalVerificationEvidenceSchema],
        diagnostics: Sequence[VerificationEvidenceDiagnosticSchema] = (),
    ) -> SignalVerificationEvidenceSet:
        summary = build_signal_verification_evidence_summary(evidence).model_dump()
        payload = [item.model_dump() for item in diagnostics]

        evidence_set = await self.get_signal_verification_evidence_set(
            workspace_id=workspace_id,
            test_run_id=test_run_id,
        )
        if evidence_set is None:
            evidence_set = SignalVerificationEvidenceSet(
                workspace_id=int(workspace_id),
                test_run_id=str(test_run_id),
                summary=summary,
                diagnostics=payload,
            )
            self.db.add(evidence_set)
        else:
            evidence_set.summary = summary
            evidence_set.diagnostics = payload

        await self.db.flush()
        return evidence_set
