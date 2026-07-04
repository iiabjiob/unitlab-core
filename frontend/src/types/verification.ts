export interface VerificationEvidenceDiagnostic {
  code: string
  message: string
  severity?: string | null
  details?: Record<string, unknown> | null
}

export type VerificationConfidence =
  | "exact_iec61850"
  | "exact_report_match"
  | "discovery_match"
  | "simulated_fallback"
  | "simulated"
  | "degraded"
  | "unknown"

export interface VerificationTarget {
  signal_id: number
  signal_reference: string
  signal_path: string
  endpoint_id?: string | null
  expected_feedback_path?: string | null
  source_row_index?: number | null
  source_kind?: string | null
  source_reason?: string | null
  timeout_ms: number
  window_ms: number
  protocol?: string | null
  protocol_metadata: Record<string, unknown>
  coverage_state: "exact" | "partial" | "uncovered"
  coverage_reason?: string | null
  allocation_id?: number | null
  channel_id?: number | null
  channel_label?: string | null
  unit_id?: string | null
  source_row_id?: string | null
}

export interface VerificationSubscriptionPlanCoverage {
  total_targets: number
  covered_targets: number
  partially_covered_targets: number
  uncovered_targets: number
  groups_count: number
  endpoints_count: number
  planning_quality: string
}

export interface VerificationSubscriptionPlan {
  plan_id: string
  selected_signal_ids: number[]
  targets: VerificationTarget[]
  groups: Array<Record<string, unknown>>
  uncovered_targets: Array<Record<string, unknown>>
  planning_diagnostics: string[]
  coverage: VerificationSubscriptionPlanCoverage
}

export interface VerificationExecutionContext {
  project_id: number
  signal_list_revision_id: number
  planner_version: string
  runtime_version: string
  policy_version: string
  selected_group_id?: string | null
  scd_revision_id?: number | null
  discovery_snapshot_id?: number | null
  operator_id?: string | null
  transport_override_host?: string | null
  transport_override_port?: number | null
  created_at?: string | null
  triggered_at?: string | null
}

export interface VerificationNetworkInterface {
  interface_name: string
  local_ip: string
  netmask?: string | null
  network?: string | null
  matches_target: boolean
  recommended: boolean
  reason?: string | null
}

export interface VerificationNetworkPreflightGroup {
  group_id: string
  endpoint_id?: string | null
  ied_name?: string | null
  access_point_name?: string | null
  target_host?: string | null
  target_port?: number | null
  readiness_state: "ready" | "attention_required" | "unknown"
  operator_hint: string
  recommended_interface_name?: string | null
  recommended_local_ip?: string | null
  recommended_netmask?: string | null
  observed_ips: string[]
  observed_network_hints: string[]
  interfaces: VerificationNetworkInterface[]
  diagnostics: VerificationEvidenceDiagnostic[]
}

export interface VerificationNetworkPreflight {
  workspace_id: number
  test_run_id?: string | null
  requested_runtime_version: "simulator" | "mms"
  recommended_runtime_version: "simulator" | "mms"
  overall_state: "ready" | "attention_required" | "unknown"
  overall_hint: string
  groups: VerificationNetworkPreflightGroup[]
  diagnostics: VerificationEvidenceDiagnostic[]
}

export interface VerificationNetworkPreflightResponse {
  preflight: VerificationNetworkPreflight
}

export interface VerificationMmsReachabilityTarget {
  host: string
  port?: number
}

export interface VerificationMmsReachabilityRequest {
  targets: VerificationMmsReachabilityTarget[]
  timeout_ms?: number
  concurrency?: number
}

export interface VerificationMmsReachabilityResult {
  host: string
  port: number
  reachable: boolean
  checked_at: string
  error?: string | null
  check_kind?: "tcp_connect"
  failure_code?: "unreachable" | "mms_unavailable" | "network_unreachable" | "probe_failed" | null
}

export interface VerificationMmsReachabilityResponse {
  results: VerificationMmsReachabilityResult[]
}

export interface VerificationExternalIedTarget {
  ip: string
  port?: number
  signal_ids: number[]
}

export interface VerificationExternalIedTargetsRequest {
  targets: VerificationExternalIedTarget[]
}

export interface VerificationExternalIedDiscoveryTreeSignal {
  reference: string
  fc?: string | null
}

export interface VerificationExternalIedDiscoveryTreeDataset {
  reference: string
  signals: VerificationExternalIedDiscoveryTreeSignal[]
}

export interface VerificationExternalIedDiscoveryTreeReport {
  reference: string
  name: string
  kind: string
  dataset_reference?: string | null
  dataset?: VerificationExternalIedDiscoveryTreeDataset | null
}

export interface VerificationExternalIedDiscoveryTreeResponse {
  endpoint: string
  model_fingerprint?: string | null
  reports: VerificationExternalIedDiscoveryTreeReport[]
}

export interface VerificationExternalIedManualReportRequest {
  report_reference: string
  report_name?: string | null
  report_kind?: string | null
  dataset_reference?: string | null
}

export interface VerificationExternalIedManualReportResponse {
  workspace_id: number
  endpoint: string
  report_reference: string
  enabled: boolean
  status: string
  message?: string | null
}

export interface VerificationAutoRunStartPayload {
  signal_ids: number[]
  execution_context: VerificationExecutionContext
  client_id?: string
  test_run_id?: string | null
}

export interface VerificationEvidence {
  evidence_id: string
  signal_id: number
  signal_path: string
  expected_path: string
  actual_report_path?: string | null
  source_ied?: string | null
  endpoint_id?: string | null
  rpt_id?: string | null
  dataset?: string | null
  observed_at?: string | null
  latency_ms?: number | null
  quality?: string | null
  freshness?: "live" | "stale" | "unknown" | null
  evidence_status: "observed" | "stale" | "timeout" | "invalid" | "late" | "out_of_window"
  reason_code: string
  source_generation?: number | null
  source_report_sequence_generation?: number | null
  source_report_sequence_number?: number | null
  source_report_sub_sequence_number?: number | null
  report_reason?: string | null
  signal_value?: unknown | null
  timestamp_summary?: Record<string, unknown> | null
  stale_reason?: string | null
  evidence_kind?: string | null
  diagnostics: VerificationEvidenceDiagnostic[]
}

export interface VerificationEvidenceSummary {
  evidence_count: number
  observed_count: number
  stale_count: number
  timeout_count: number
  invalid_count: number
  late_count: number
  out_of_window_count: number
  source_generation?: number | null
}

export interface VerificationEvidenceSet {
  test_run_id: string
  evidence: VerificationEvidence[]
  summary: VerificationEvidenceSummary
  diagnostics: VerificationEvidenceDiagnostic[]
}

export interface VerificationSessionSnapshot {
  session_id: string
  endpoint_id: string
  runtime_state: string
  connection_generation: number
  discovery_status: string
  last_error?: string | null
  diagnostic_code?: string | null
}

export interface VerificationSubscriptionSnapshot {
  subscription_id: string
  session_id: string
  endpoint_id: string
  group_id?: string | null
  report_control_reference?: string | null
  report_control_name?: string | null
  data_set_reference?: string | null
  subscription_state: "pending" | "reserving" | "enabled" | "reporting" | "reconnecting" | "degraded" | "closed" | "failed"
  report_health: "unknown" | "healthy" | "degraded"
  last_report_at?: string | null
  gi_requested?: boolean
  last_report_value_count?: number
  last_report_values?: Array<{
    index?: number | null
    reference?: string | null
    data_reference?: string | null
    value?: unknown
    reason?: string | null
    timestamp?: string | null
  }>
  current_rptena_owner?: string | null
  stale_signal_count?: number | null
  last_error?: string | null
  diagnostic_code?: string | null
  diagnostics: VerificationEvidenceDiagnostic[]
}

export interface VerificationStep {
  step_id: string
  signal_id: number
  target_index: number
  session_id: string
  subscription_id: string
  group_id?: string | null
  step_state: "draft" | "planned" | "armed" | "running" | "awaiting_confirmation" | "completing" | "completed" | "aborted" | "failed"
  expected_path: string
  expected_window_ms: number
  freshness?: "live" | "stale" | "unknown" | null
  evidence_status: "none" | "observed" | "stale" | "timeout" | "invalid" | "late" | "out_of_window"
  verdict_state: "pending" | "pass" | "fail" | "inconclusive" | "aborted"
  evidence_ids: string[]
  actual_report_path?: string | null
  source_session_id?: string | null
  source_subscription_id?: string | null
  source_generation?: number | null
  source_report_rpt_id?: string | null
  source_report_dat_set?: string | null
  verification_confidence: VerificationConfidence
  confidence_reason: string
  triggered_at?: string | null
  observed_at?: string | null
  latency_ms?: number | null
  reason?: string | null
  diagnostics: VerificationEvidenceDiagnostic[]
}

export interface VerificationRun {
  test_run_id: string
  verification_targets: VerificationTarget[]
  subscription_plan: VerificationSubscriptionPlan
  session_snapshots: VerificationSessionSnapshot[]
  subscription_snapshots: VerificationSubscriptionSnapshot[]
  evidence_set: VerificationEvidenceSet
  execution_context: VerificationExecutionContext
  workflow_state: "draft" | "planned" | "preparing" | "armed" | "running" | "awaiting_confirmation" | "completing" | "completed" | "aborted" | "failed"
  verdict_state: "pending" | "pass" | "fail" | "inconclusive" | "aborted"
  verification_confidence: VerificationConfidence
  confidence_reason: string
  selected_group_id?: string | null
  operator_id?: string | null
  triggered_at?: string | null
  completed_at?: string | null
  runtime_state?: string | null
  runtime_summary?: Record<string, unknown> | null
  reason?: string | null
  diagnostics: VerificationEvidenceDiagnostic[]
  verification_steps: VerificationStep[]
}

export interface VerificationVerdictExplanationSignal {
  signal_id: number
  signal_reference: string
  signal_path: string
  expected_path: string
  observed_path?: string | null
  source_ied?: string | null
  endpoint_id?: string | null
  rpt_id?: string | null
  dataset?: string | null
  evidence_status: "observed" | "stale" | "timeout" | "invalid" | "late" | "out_of_window"
  verdict_state: "pending" | "pass" | "fail" | "inconclusive" | "aborted"
  latency_ms?: number | null
  reason?: string | null
  diagnostics: VerificationEvidenceDiagnostic[]
}

export interface VerificationVerdictExplanation {
  test_run_id: string
  verdict_state: "pending" | "pass" | "fail" | "inconclusive" | "aborted"
  verification_confidence: VerificationConfidence
  confidence_reason: string
  headline: string
  summary: string
  signals: VerificationVerdictExplanationSignal[]
  diagnostics: VerificationEvidenceDiagnostic[]
}

export interface VerificationRunDetailResponse {
  test_run_id: string
  verification_run: VerificationRun
  verdict_explanation: VerificationVerdictExplanation
}

export interface VerificationRuntimeOrchestrationResponse {
  orchestration_id: string
  verification_run: VerificationRun
}
