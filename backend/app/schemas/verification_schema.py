from __future__ import annotations

from datetime import datetime
from typing import Any, Literal

from pydantic import BaseModel, Field


class VerificationTargetSchema(BaseModel):
    signal_id: int
    signal_reference: str
    signal_path: str
    endpoint_id: str | None = None
    expected_feedback_path: str | None = None
    source_row_index: int | None = None
    source_kind: str | None = None
    source_reason: str | None = None
    timeout_ms: int = Field(default=5000, ge=1)
    window_ms: int = Field(default=1000, ge=0)
    protocol: str | None = None
    protocol_metadata: dict[str, Any] = Field(default_factory=dict)
    coverage_state: Literal["exact", "partial", "uncovered"] = "uncovered"
    coverage_reason: str | None = None
    allocation_id: int | None = None
    channel_id: int | None = None
    channel_label: str | None = None
    unit_id: str | None = None
    source_row_id: str | None = None


class VerificationSubscriptionPlanGroupSchema(BaseModel):
    group_id: str = ""
    endpoint_id: str | None = None
    ied_name: str | None = None
    access_point_name: str | None = None
    report_control_reference: str | None = None
    report_control_name: str | None = None
    report_kind: str | None = None
    rpt_id: str | None = None
    data_set_reference: str | None = None
    target_indexes: list[int] = Field(default_factory=list)
    reason: str = ""
    source_classification: Literal["from SCD", "from discovery", "fallback", "not found"] = "fallback"
    source_reason: str | None = None


class VerificationSubscriptionPlanUncoveredTargetSchema(BaseModel):
    target_index: int
    reason: str
    detail: str


class VerificationSubscriptionPlanCoverageSchema(BaseModel):
    total_targets: int = 0
    covered_targets: int = 0
    partially_covered_targets: int = 0
    uncovered_targets: int = 0
    groups_count: int = 0
    endpoints_count: int = 0
    planning_quality: str = "partial"


class VerificationSubscriptionPlanSchema(BaseModel):
    plan_id: str = ""
    selected_signal_ids: list[int] = Field(default_factory=list)
    targets: list[VerificationTargetSchema] = Field(default_factory=list)
    groups: list[VerificationSubscriptionPlanGroupSchema] = Field(default_factory=list)
    uncovered_targets: list[VerificationSubscriptionPlanUncoveredTargetSchema] = Field(default_factory=list)
    planning_diagnostics: list[str] = Field(default_factory=list)
    coverage: VerificationSubscriptionPlanCoverageSchema


class VerificationEvidenceDiagnosticSchema(BaseModel):
    code: str
    message: str
    severity: str | None = None
    details: dict[str, Any] | None = None


class SignalVerificationEvidenceSchema(BaseModel):
    evidence_id: str
    signal_id: int
    signal_path: str
    expected_path: str
    actual_report_path: str | None = None
    source_ied: str | None = None
    endpoint_id: str | None = None
    rpt_id: str | None = None
    dataset: str | None = None
    observed_at: datetime | None = None
    latency_ms: int | None = None
    quality: str | None = None
    freshness: Literal["live", "stale", "unknown"] | None = None
    evidence_status: Literal["observed", "stale", "timeout", "invalid", "late", "out_of_window"]
    reason_code: str
    source_generation: int | None = None
    source_report_sequence_generation: int | None = None
    source_report_sequence_number: int | None = None
    source_report_sub_sequence_number: int | None = None
    report_reason: str | None = None
    signal_value: Any | None = None
    timestamp_summary: dict[str, Any] | None = None
    stale_reason: str | None = None
    evidence_kind: str | None = None
    diagnostics: list[VerificationEvidenceDiagnosticSchema] = Field(default_factory=list)


class SignalVerificationEvidenceSetSummarySchema(BaseModel):
    evidence_count: int = 0
    observed_count: int = 0
    stale_count: int = 0
    timeout_count: int = 0
    invalid_count: int = 0
    late_count: int = 0
    out_of_window_count: int = 0
    source_generation: int | None = None


class SignalVerificationEvidenceSetSchema(BaseModel):
    test_run_id: str
    evidence: list[SignalVerificationEvidenceSchema] = Field(default_factory=list)
    summary: SignalVerificationEvidenceSetSummarySchema
    diagnostics: list[VerificationEvidenceDiagnosticSchema] = Field(default_factory=list)
