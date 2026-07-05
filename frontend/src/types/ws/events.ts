import type { Channel } from "../channel"
import type { DeviceHeartbeatDiagSnapshot, DeviceHeartbeatFastSnapshot } from "../device"
import type { SystemHealthResponse, SystemStatus } from "../health"
import type { TimeStatus } from "../time"
import type { CoreNetworkSnapshot } from "../coreNetwork"
import type { CoreNtpSnapshot } from "../coreNtp"
import type { CoreDiagnosticsSnapshot } from "../coreDiagnostics"
import type { CoreProvisionSnapshot } from "../coreProvision"
import type { SequenceRuntimeState } from "../sequences"
import type {
  VerificationExternalIedManualReportValue,
  VerificationExternalIedManualSignalState,
} from "../verification"
// ---------------------------------------------------------------------
// WS channels (Backend → Frontend)
// ---------------------------------------------------------------------

export enum WSChannel {
  SYSTEM_INFO    = "system/info",
  DEVICE_STATE    = "devices/state",
  DEVICE_REGISTER = "devices/register",
  DEVICE_RESP     = "devices/resp",
  DEVICE_STATUS   = "devices/status",
  EXTERNAL_IED_STATUS = "external-ieds/status",
  EXTERNAL_IED_MANUAL_REPORTS = "external-ieds/manual-reports",
  TIME_STATUS     = "time/status",
}

// ---------------------------------------------------------------------
// Protocol constants mirror backend/protocol/modes.py
// ---------------------------------------------------------------------
export enum StateMode {
  STATE_SINGLE_BIT   = 0x12,
  STATE_ALL_BIT      = 0x13,
  STATE_CHANGED_BIT  = 0x14,
  STATE_LATCHED_BIT  = 0x15,
  DIAG_DI_BIT        = 0x17,
  DIAG_ALL_BIT       = 0x19,
  STATE_SINGLE_FLOAT = 0x1C,
  DIAG_AO_FLOAT      = 0x1E,
}

export interface DeviceDiagnosticsPayload {
  open_mask: number
  fault_mask: number
  soft_mask: number
}

export interface DeviceDeltaPayload {
  changed: number
  state: number
}

export interface DeviceDiDiagnosticsPayload {
  seen: number
  stuck: number
  lost: number
  latched: number
  latched_changed: number
  latched_cause: number
}

export interface DeviceDiLatchedPayload {
  latched: number
  changed: number
  cause: number
}

export interface DeviceAoDiagnosticsPayload {
  valid_mask: number
  pending_mask: number
  fault_mask: number
  error_mask: number
}

// ---------------------------------------------------------------------
// RESP enums (Backend → Frontend)
// ---------------------------------------------------------------------
export enum RespStatus {
  OK = "OK",
  BAD_REQUEST = "BAD_REQUEST",
  UNSUPPORTED = "UNSUPPORTED",
  BUSY = "BUSY",
  TIMEOUT = "TIMEOUT",
  INTERNAL_ERROR = "INTERNAL_ERROR",
  INVALID_STATE = "INVALID_STATE",
}

export enum RespError {
  NONE = "NONE",
  ARG_RANGE = "ARG_RANGE",
  ARG_VALUE = "ARG_VALUE",
  NOT_READY = "NOT_READY",
  STORAGE_FAIL = "STORAGE_FAIL",
  TRANSPORT_FAIL = "TRANSPORT_FAIL",
  PERMISSION = "PERMISSION",
  HW_FAILURE = "HW_FAILURE",
}

// ---------------------------------------------------------------------
// Device Events
// ---------------------------------------------------------------------

export interface DeviceStateEvent {
  channel: WSChannel.DEVICE_STATE
  unit_id: string
  timestamp: number
  mode: StateMode
  payload: Record<string, any>
}

export interface DeviceRegisterEvent {
  channel: WSChannel.DEVICE_REGISTER
  id: number
  unit_id: string
  device_type: "do" | "di" | "ao"
  num_channels?: number | null
  firmware_version?: string | null
  name?: string | null
  status: "online" | "offline"
  last_seen?: number | null
  registered_at?: number | null
  created?: boolean
  channels?: Channel[] | null
}

export interface DeviceRespEvent {
  channel: WSChannel.DEVICE_RESP
  unit_id: string
  packet_id: number
  status: RespStatus
  error: RespError
  timestamp: number
}

export interface DeviceHeartbeatEvent {
  channel: WSChannel.DEVICE_STATUS
  unit_id: string
  status: "online" | "offline"
  last_seen: number
  heartbeat_kind?: "fast" | "diag" | null
  heartbeat_fast?: DeviceHeartbeatFastSnapshot | null
  heartbeat_diag?: DeviceHeartbeatDiagSnapshot | null
}

export type ExternalIedStatus = "not_applicable" | "unknown" | "expected" | "reachable" | "offline"
export type ExternalIedCheckKind = "none" | "tcp_connect"
export type ExternalIedFailureCode = "unreachable" | "mms_unavailable" | "network_unreachable" | "probe_failed"
export type ExternalIedDiscoveryState = "NeverDiscovered" | "Queued" | "Running" | "Succeeded" | "Failed" | "RetryWaiting" | "Cancelled" | "Stale"
export type ExternalIedPlanningState = "NotPlanned" | "Queued" | "Running" | "Ready" | "Partial" | "Failed" | "Stale" | "WaitingForDiscovery"
export type ExternalIedPlanningSignalStatus = "matched" | "unmatched" | "ambiguous" | "not_planned" | "stale"

export interface ExternalIedStatusRecord {
  ip: string
  port: number
  status: ExternalIedStatus
  signal_ids: number[]
  last_checked_at?: string | null
  last_error?: string | null
  check_kind: ExternalIedCheckKind
  failure_code?: ExternalIedFailureCode | null
  discovery_state?: ExternalIedDiscoveryState
  discovery_retry_at_ms?: number | null
  discovery_last_error?: string | null
  discovery_updated_at_ms?: number | null
  discovery_ready_for_verification?: boolean
  discovery_device_identity?: string | null
  discovery_vendor?: string | null
  discovery_model?: string | null
  discovery_datasets?: number | null
  discovery_rcbs?: number | null
  discovery_model_signals?: number | null
  planning_state?: ExternalIedPlanningState
  planning_updated_at_ms?: number | null
  planning_matched_count?: number
  planning_unmatched_count?: number
  planning_ambiguous_count?: number
  planning_last_error?: string | null
}

export interface ExternalIedStatusSnapshotEvent {
  channel: WSChannel.EXTERNAL_IED_STATUS
  event: "external_ied_status_snapshot"
  workspace_id: number
  devices: ExternalIedStatusRecord[]
  removed_signal_ids: number[]
  emitted_at: string
}

export interface ExternalIedStatusChangedEvent {
  channel: WSChannel.EXTERNAL_IED_STATUS
  event: "external_ied_status_changed"
  workspace_id: number
  ip: string
  port: number
  old_status: ExternalIedStatus
  new_status: ExternalIedStatus
  signal_ids: number[]
  checked_at: string
  check_kind: ExternalIedCheckKind
  failure_code?: ExternalIedFailureCode | null
  error?: string | null
  discovery_state?: ExternalIedDiscoveryState
  discovery_retry_at_ms?: number | null
  discovery_last_error?: string | null
  discovery_updated_at_ms?: number | null
  discovery_ready_for_verification?: boolean
  discovery_device_identity?: string | null
  discovery_vendor?: string | null
  discovery_model?: string | null
  discovery_datasets?: number | null
  discovery_rcbs?: number | null
  discovery_model_signals?: number | null
  planning_state?: ExternalIedPlanningState
  planning_updated_at_ms?: number | null
  planning_matched_count?: number
  planning_unmatched_count?: number
  planning_ambiguous_count?: number
  planning_last_error?: string | null
}

export interface ExternalIedPlanningSignalResult {
  signal_id: number
  endpoint: string
  status: ExternalIedPlanningSignalStatus
  address?: string | null
  reason?: string | null
  ied_identity?: string | null
  fcda_reference?: string | null
  dataset_reference?: string | null
  rcb_reference?: string | null
  rcb_name?: string | null
}

export interface ExternalIedPlanningEndpointRecord {
  endpoint: string
  ip: string
  port: number
  state: ExternalIedPlanningState
  planning_fingerprint?: string | null
  model_fingerprint?: string | null
  updated_at_ms?: number | null
  matched_count: number
  unmatched_count: number
  ambiguous_count: number
  signal_ids: number[]
  last_error?: string | null
}

export interface ExternalIedPlanningSnapshotEvent {
  channel: WSChannel.EXTERNAL_IED_STATUS
  event: "external_ied_planning_snapshot"
  workspace_id: number
  endpoints: ExternalIedPlanningEndpointRecord[]
  signal_results: ExternalIedPlanningSignalResult[]
  removed_signal_ids: number[]
  emitted_at: string
}

export interface ExternalIedPlanningChangedEvent {
  channel: WSChannel.EXTERNAL_IED_STATUS
  event: "external_ied_planning_changed"
  workspace_id: number
  endpoint: ExternalIedPlanningEndpointRecord
  signal_results: ExternalIedPlanningSignalResult[]
  removed_signal_ids: number[]
  emitted_at: string
}

export interface ExternalIedManualReportValuesChangedEvent {
  channel: WSChannel.EXTERNAL_IED_MANUAL_REPORTS
  event: "external_ied_manual_report_values_changed"
  workspace_id: number
  endpoint: string
  ip: string
  port: number
  report_reference: string
  lease_id: string
  status: string
  signal_states: VerificationExternalIedManualSignalState[]
  report_values: VerificationExternalIedManualReportValue[]
  emitted_at: string
}

export interface SystemHealthChangedEvent {
  channel: WSChannel.SYSTEM_INFO
  event: "system_health_changed"
  previous_status: SystemStatus | null
  current_status: SystemStatus
  changed_at: string
  issues: string[]
  diff: {
    workers: string[]
  }
  snapshot: SystemHealthResponse
}

export interface CoreNetworkStateWsEvent {
  channel: WSChannel.SYSTEM_INFO
  event: "core_network_state"
  snapshot: CoreNetworkSnapshot
  changed_at: string
}

export interface CoreNtpStateWsEvent {
  channel: WSChannel.SYSTEM_INFO
  event: "core_ntp_state"
  snapshot: CoreNtpSnapshot
  changed_at: string
}

export interface CoreDiagnosticsStateWsEvent {
  channel: WSChannel.SYSTEM_INFO
  event: "core_diagnostics_state"
  snapshot: CoreDiagnosticsSnapshot
  changed_at: string
}

export interface CoreProvisionStateWsEvent {
  channel: WSChannel.SYSTEM_INFO
  event: "core_provision_state"
  snapshot: CoreProvisionSnapshot
  changed_at: string
}

export interface SignalAllocationJobEvent {
  channel: WSChannel.SYSTEM_INFO
  event: "signal_allocation_job"
  job_id: string
  workspace_id: number
  operation: "auto_allocate" | "bulk_update" | "test_run" | string
  status: "queued" | "running" | "paused" | "cancelling" | "cancelled" | "succeeded" | "failed"
  progress_total: number
  progress_done: number
  message: string | null
  error: string | null
  result: Record<string, unknown>
  created_at: string
  updated_at: string
}

export interface SignalTestRunJobEvent {
  channel: WSChannel.SYSTEM_INFO
  event: "signal_test_run_job"
  job_id: string
  workspace_id: number
  operation: "test_run" | string
  status: "queued" | "running" | "paused" | "cancelling" | "cancelled" | "succeeded" | "failed"
  progress_total: number
  progress_done: number
  message: string | null
  error: string | null
  result: Record<string, unknown>
  created_at: string
  updated_at: string
}

export interface SignalTestRuntimePatchEvent {
  channel: WSChannel.SYSTEM_INFO
  event: "signal_test_runtime_patch"
  job_id: string
  workspace_id: number
  patch_type: "tested_at"
  tested_at_by_signal: Record<string, string>
  emitted_at: string
}

export type SignalRowsPatchSource = "allocation" | "test_runtime" | "device_health"

export interface SignalRowsPatchedRowPatch {
  row_id?: string | null
  signal_id: number
  changes: Record<string, unknown>
  columns?: string[] | null
}

export interface SignalRowsPatchedEvent {
  channel: WSChannel.SYSTEM_INFO
  event: "signal_rows_patched"
  workspace_id: number
  sequence: number
  source: SignalRowsPatchSource
  patches: SignalRowsPatchedRowPatch[]
  requires_full_reload?: boolean
  emitted_at?: string
}

export interface TimeStatusEvent extends TimeStatus {
  channel: WSChannel.TIME_STATUS
}

export interface SequenceEventBase {
  topic: "sequence"
  sequence_id: number
  run_id: number
}

export interface SequenceStartedEvent extends SequenceEventBase {
  event: "started"
  total_steps: number
  runtime?: SequenceRuntimeState | null
}

export interface SequenceProgressEvent extends SequenceEventBase {
  event: "progress"
  step_index: number
  step_id: number
  step_type: string
  progress_scope: "step" | "nested_step"
  step_elapsed_ms: number
  run_elapsed_ms: number
  completed_steps: number[]
  runtime?: SequenceRuntimeState | null
}

export interface SequenceStepErrorEvent extends SequenceEventBase {
  event: "step_error"
  step_index: number
  step_id: number
  message: string
  runtime?: SequenceRuntimeState | null
}

export interface SequenceErrorEvent extends SequenceEventBase {
  event: "error"
  message: string
  runtime?: SequenceRuntimeState | null
}

export interface SequenceStoppingEvent extends SequenceEventBase {
  event: "stopping"
  current_step_index: number
  total_steps: number
  runtime?: SequenceRuntimeState | null
}

export interface SequenceStoppedEvent extends SequenceEventBase {
  event: "stopped"
  runtime?: SequenceRuntimeState | null
}

export interface SequenceCompletedEvent extends SequenceEventBase {
  event: "completed"
  elapsed_ms: number
  runtime?: SequenceRuntimeState | null
}

export type SequenceWsEvent =
  | SequenceStartedEvent
  | SequenceProgressEvent
  | SequenceStepErrorEvent
  | SequenceErrorEvent
  | SequenceStoppingEvent
  | SequenceStoppedEvent
  | SequenceCompletedEvent


export type ChannelWSEvent =
  | SystemHealthChangedEvent
  | SignalAllocationJobEvent
  | CoreNetworkStateWsEvent
  | CoreNtpStateWsEvent
  | CoreDiagnosticsStateWsEvent
  | CoreProvisionStateWsEvent
  | SignalTestRunJobEvent
  | SignalTestRuntimePatchEvent
  | SignalRowsPatchedEvent
  | ExternalIedStatusSnapshotEvent
  | ExternalIedStatusChangedEvent
  | ExternalIedManualReportValuesChangedEvent
  | DeviceStateEvent
  | DeviceRegisterEvent
  | DeviceRespEvent
  | DeviceHeartbeatEvent
  | TimeStatusEvent

export type WSEvent = ChannelWSEvent | SequenceWsEvent
