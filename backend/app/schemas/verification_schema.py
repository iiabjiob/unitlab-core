from __future__ import annotations

from datetime import datetime
from typing import Any, Literal

from pydantic import BaseModel, Field, model_validator

VerificationConfidenceLevel = Literal[
    "exact_iec61850",
    "exact_report_match",
    "discovery_match",
    "simulated_fallback",
    "simulated",
    "degraded",
    "unknown",
]


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


class PlannerConfidenceSignalSchema(BaseModel):
    signal_index: int
    signal_id: int
    signal_reference: str
    endpoint_id: str | None = None
    expected_feedback_path: str | None = None
    report_control_reference: str | None = None
    report_control_name: str | None = None
    data_set_reference: str | None = None
    coverage_state: Literal["exact", "partial", "uncovered"]
    source_classification: Literal["from SCD", "from discovery", "fallback", "not found"]
    confidence_state: Literal["strong", "watch", "risk", "uncovered"]
    diagnostics: list[str] = Field(default_factory=list)


class PlannerConfidenceReportSchema(BaseModel):
    plan_id: str
    total_targets: int = 0
    covered_targets: int = 0
    partially_covered_targets: int = 0
    uncovered_targets: int = 0
    coverage_percentage: int = 0
    confidence_percentage: int = 0
    groups_count: int = 0
    endpoints_count: int = 0
    planning_quality: str = "partial"
    risk_level: Literal["low", "medium", "high"] = "medium"
    source_classification_counts: dict[str, int] = Field(default_factory=dict)
    signals: list[PlannerConfidenceSignalSchema] = Field(default_factory=list)
    diagnostics: list[str] = Field(default_factory=list)


class VerificationEvidenceDiagnosticSchema(BaseModel):
    code: str
    message: str
    severity: str = "info"
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


class VerificationExecutionContextSchema(BaseModel):
    project_id: int
    signal_list_revision_id: int
    planner_version: str
    runtime_version: str
    policy_version: str
    selected_group_id: str | None = None
    scd_revision_id: int | None = None
    discovery_snapshot_id: int | None = None
    operator_id: str | None = None
    transport_override_host: str | None = None
    transport_override_port: int | None = None
    created_at: datetime | None = None
    triggered_at: datetime | None = None


class VerificationNetworkInterfaceSchema(BaseModel):
    interface_name: str
    local_ip: str
    netmask: str | None = None
    network: str | None = None
    matches_target: bool = False
    recommended: bool = False
    reason: str | None = None


class VerificationNetworkPreflightGroupSchema(BaseModel):
    group_id: str
    endpoint_id: str | None = None
    ied_name: str | None = None
    access_point_name: str | None = None
    target_host: str | None = None
    target_port: int | None = None
    readiness_state: Literal["ready", "attention_required", "unknown"]
    operator_hint: str
    recommended_interface_name: str | None = None
    recommended_local_ip: str | None = None
    recommended_netmask: str | None = None
    observed_ips: list[str] = Field(default_factory=list)
    observed_network_hints: list[str] = Field(default_factory=list)
    interfaces: list[VerificationNetworkInterfaceSchema] = Field(default_factory=list)
    diagnostics: list[VerificationEvidenceDiagnosticSchema] = Field(default_factory=list)


class VerificationNetworkPreflightSchema(BaseModel):
    workspace_id: int
    test_run_id: str | None = None
    requested_runtime_version: Literal["simulator", "mms"]
    recommended_runtime_version: Literal["simulator", "mms"]
    overall_state: Literal["ready", "attention_required", "unknown"]
    overall_hint: str
    groups: list[VerificationNetworkPreflightGroupSchema] = Field(default_factory=list)
    diagnostics: list[VerificationEvidenceDiagnosticSchema] = Field(default_factory=list)


class VerificationNetworkPreflightResponseSchema(BaseModel):
    preflight: VerificationNetworkPreflightSchema


class VerificationMmsReachabilityTargetSchema(BaseModel):
    host: str = Field(min_length=1, max_length=255)
    port: int = Field(default=102, ge=1, le=65535)


class VerificationMmsReachabilityRequestSchema(BaseModel):
    targets: list[VerificationMmsReachabilityTargetSchema] = Field(default_factory=list, min_length=1, max_length=64)
    timeout_ms: int = Field(default=1200, ge=100, le=5000)
    concurrency: int = Field(default=4, ge=1, le=16)


class VerificationMmsReachabilityResultSchema(BaseModel):
    host: str
    port: int
    reachable: bool
    checked_at: str
    error: str | None = None
    check_kind: Literal["tcp_connect"] = "tcp_connect"
    failure_code: Literal["unreachable", "mms_unavailable", "network_unreachable", "probe_failed"] | None = None


class VerificationMmsReachabilityResponseSchema(BaseModel):
    results: list[VerificationMmsReachabilityResultSchema] = Field(default_factory=list)


class VerificationExternalIedTargetSchema(BaseModel):
    ip: str = Field(min_length=1, max_length=64)
    port: int = Field(default=102, ge=1, le=65535)
    signal_ids: list[int] = Field(default_factory=list, max_length=20000)


class VerificationExternalIedTargetsRequestSchema(BaseModel):
    targets: list[VerificationExternalIedTargetSchema] = Field(default_factory=list, max_length=512)


class VerificationExternalIedDiscoveryTreeSignalSchema(BaseModel):
    reference: str
    fc: str | None = None


class VerificationExternalIedDiscoveryTreeDatasetSchema(BaseModel):
    reference: str
    signals: list[VerificationExternalIedDiscoveryTreeSignalSchema] = Field(default_factory=list)


class VerificationExternalIedDiscoveryTreeReportSchema(BaseModel):
    reference: str
    name: str
    kind: str = "unknown"
    dataset_reference: str | None = None
    dataset: VerificationExternalIedDiscoveryTreeDatasetSchema | None = None


class VerificationExternalIedDiscoveryTreeResponseSchema(BaseModel):
    endpoint: str
    model_fingerprint: str | None = None
    reports: list[VerificationExternalIedDiscoveryTreeReportSchema] = Field(default_factory=list)


class VerificationExternalIedManualReportRequestSchema(BaseModel):
    report_reference: str = Field(min_length=1, max_length=512)
    report_name: str | None = Field(default=None, max_length=256)
    report_kind: str | None = Field(default=None, max_length=64)
    dataset_reference: str | None = Field(default=None, max_length=512)


class VerificationExternalIedManualReportResponseSchema(BaseModel):
    workspace_id: int
    endpoint: str
    report_reference: str
    enabled: bool
    status: str
    message: str | None = None
    lease_id: str | None = None
    owner: str | None = None
    created_at: str | None = None
    renewed_at: str | None = None
    expires_at: str | None = None
    signal_states: list[dict[str, Any]] = Field(default_factory=list)
    report_values: list[dict[str, Any]] = Field(default_factory=list)


class VerificationSessionSnapshotSchema(BaseModel):
    session_id: str
    endpoint_id: str
    runtime_state: str
    connection_generation: int
    discovery_status: str
    last_error: str | None = None
    diagnostic_code: str | None = None


class VerificationSubscriptionSnapshotSchema(BaseModel):
    subscription_id: str
    session_id: str
    endpoint_id: str
    group_id: str | None = None
    report_control_reference: str | None = None
    report_control_name: str | None = None
    data_set_reference: str | None = None
    subscription_state: Literal["pending", "reserving", "enabled", "reporting", "reconnecting", "degraded", "closed", "failed"]
    report_health: Literal["unknown", "healthy", "degraded"]
    last_report_at: datetime | None = None
    gi_requested: bool = False
    last_report_value_count: int = 0
    last_report_values: list[dict[str, Any]] = Field(default_factory=list)
    current_rptena_owner: str | None = None
    stale_signal_count: int | None = None
    last_error: str | None = None
    diagnostic_code: str | None = None
    diagnostics: list[VerificationEvidenceDiagnosticSchema] = Field(default_factory=list)


class VerificationRecoveryStateSchema(BaseModel):
    session_id: str
    endpoint_id: str
    runtime_state: Literal["reporting", "reconnecting", "degraded", "closed", "failed", "discovering", "subscribing"]
    desired_state: Literal["reporting", "reconnecting", "discovering", "subscribing"]
    active_generation: int
    recovery_reason: Literal[
        "disconnect",
        "association_lost",
        "report_health_degraded",
        "stale_generation",
        "subscription_lost",
        "timeout",
        "user_reconnect",
        "runtime_failure",
    ] | None = None
    desired_subscription_plan_id: str
    desired_group_ids: list[str] = Field(default_factory=list)
    desired_report_controls: list[str] = Field(default_factory=list)
    desired_target_ids: list[int] = Field(default_factory=list)
    active_verification_run_id: str
    preserved_evidence_count: int = 0
    in_flight: bool | None = None
    preserved_verification_targets: list[VerificationTargetSchema] = Field(default_factory=list)
    stale_signal_count: int | None = None
    diagnostics: list[VerificationEvidenceDiagnosticSchema] = Field(default_factory=list)


class VerificationStepSchema(BaseModel):
    step_id: str
    signal_id: int
    target_index: int
    session_id: str
    subscription_id: str
    group_id: str | None = None
    step_state: Literal["draft", "planned", "armed", "running", "awaiting_confirmation", "completing", "completed", "aborted", "failed"]
    expected_path: str
    expected_window_ms: int
    freshness: Literal["live", "stale", "unknown"] | None = None
    evidence_status: Literal["none", "observed", "stale", "timeout", "invalid", "late", "out_of_window"]
    verdict_state: Literal["pending", "pass", "fail", "inconclusive", "aborted"]
    evidence_ids: list[str] = Field(default_factory=list)
    actual_report_path: str | None = None
    source_session_id: str | None = None
    source_subscription_id: str | None = None
    source_generation: int | None = None
    source_report_rpt_id: str | None = None
    source_report_dat_set: str | None = None
    verification_confidence: VerificationConfidenceLevel = "unknown"
    confidence_reason: str = "unknown"
    triggered_at: datetime | None = None
    observed_at: datetime | None = None
    latency_ms: int | None = None
    reason: str | None = None
    diagnostics: list[VerificationEvidenceDiagnosticSchema] = Field(default_factory=list)

    @model_validator(mode="before")
    @classmethod
    def _normalize_identity(cls, value: Any) -> Any:
        if not isinstance(value, dict):
            return value

        data = dict(value)
        if not data.get("session_id") and data.get("source_session_id"):
            data["session_id"] = data["source_session_id"]

        if not data.get("subscription_id"):
            subscription_id = data.get("source_subscription_id") or data.get("group_id")
            if subscription_id is None:
                session_id = data.get("session_id")
                report_reference = data.get("source_report_rpt_id") or data.get("source_report_dat_set")
                if session_id and report_reference:
                    subscription_id = f"{session_id}:{report_reference}"
            if subscription_id is not None:
                data["subscription_id"] = str(subscription_id)

        return data


class VerificationRunSchema(BaseModel):
    test_run_id: str
    verification_targets: list[VerificationTargetSchema] = Field(default_factory=list)
    subscription_plan: VerificationSubscriptionPlanSchema
    session_snapshots: list[VerificationSessionSnapshotSchema] = Field(default_factory=list)
    subscription_snapshots: list[VerificationSubscriptionSnapshotSchema] = Field(default_factory=list)
    evidence_set: SignalVerificationEvidenceSetSchema
    execution_context: VerificationExecutionContextSchema
    recovery_state: VerificationRecoveryStateSchema | None = None
    workflow_state: Literal["draft", "planned", "preparing", "armed", "running", "awaiting_confirmation", "completing", "completed", "aborted", "failed"]
    verdict_state: Literal["pending", "pass", "fail", "inconclusive", "aborted"]
    verification_confidence: VerificationConfidenceLevel = "unknown"
    confidence_reason: str = "unknown"
    selected_group_id: str | None = None
    operator_id: str | None = None
    triggered_at: datetime | None = None
    completed_at: datetime | None = None
    runtime_state: str | None = None
    runtime_summary: dict[str, Any] | None = None
    reason: str | None = None
    diagnostics: list[VerificationEvidenceDiagnosticSchema] = Field(default_factory=list)
    verification_steps: list[VerificationStepSchema] = Field(default_factory=list)


class VerificationRuntimeOrchestrationStartSchema(BaseModel):
    test_run_id: str
    verification_targets: list[VerificationTargetSchema] = Field(default_factory=list)
    subscription_plan: VerificationSubscriptionPlanSchema
    execution_context: VerificationExecutionContextSchema
    client_id: str = "unitlab-backend-simulator"


class VerificationRuntimeOrchestrationReconnectSchema(BaseModel):
    session_id: str


class VerificationRuntimeOrchestrationResponseSchema(BaseModel):
    orchestration_id: str
    verification_run: VerificationRunSchema


class VerificationRunEvidenceResponseSchema(BaseModel):
    test_run_id: str
    evidence_set: SignalVerificationEvidenceSetSchema
    evidence_rows: list[SignalVerificationEvidenceSchema] = Field(default_factory=list)
    verification_steps: list[VerificationStepSchema] = Field(default_factory=list)
    diagnostics: list[VerificationEvidenceDiagnosticSchema] = Field(default_factory=list)


class VerificationRunStepDetailsSchema(BaseModel):
    test_run_id: str
    verification_steps: list[VerificationStepSchema] = Field(default_factory=list)
    diagnostics: list[VerificationEvidenceDiagnosticSchema] = Field(default_factory=list)


class VerificationVerdictExplanationSignalSchema(BaseModel):
    signal_id: int
    signal_reference: str
    signal_path: str
    expected_path: str
    observed_path: str | None = None
    source_ied: str | None = None
    endpoint_id: str | None = None
    rpt_id: str | None = None
    dataset: str | None = None
    evidence_status: Literal["observed", "stale", "timeout", "invalid", "late", "out_of_window"]
    verdict_state: Literal["pending", "pass", "fail", "inconclusive", "aborted"]
    latency_ms: int | None = None
    reason: str | None = None
    diagnostics: list[VerificationEvidenceDiagnosticSchema] = Field(default_factory=list)


class VerificationVerdictExplanationSchema(BaseModel):
    test_run_id: str
    verdict_state: Literal["pending", "pass", "fail", "inconclusive", "aborted"]
    verification_confidence: VerificationConfidenceLevel = "unknown"
    confidence_reason: str = "unknown"
    headline: str
    summary: str
    signals: list[VerificationVerdictExplanationSignalSchema] = Field(default_factory=list)
    diagnostics: list[VerificationEvidenceDiagnosticSchema] = Field(default_factory=list)


class VerificationAutoRunStartSchema(BaseModel):
    signal_ids: list[int] = Field(default_factory=list, min_length=1)
    execution_context: VerificationExecutionContextSchema
    client_id: str = "unitlab-backend-simulator"
    test_run_id: str | None = None


class VerificationRunDetailResponseSchema(BaseModel):
    test_run_id: str
    verification_run: VerificationRunSchema
    verdict_explanation: VerificationVerdictExplanationSchema
