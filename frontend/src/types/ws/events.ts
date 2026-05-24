import type { Channel } from "../channel"
import type { DeviceHeartbeatDiagSnapshot, DeviceHeartbeatFastSnapshot } from "../device"
import type { SystemHealthResponse, SystemStatus } from "../health"
import type { TimeStatus } from "../time"
import type { CoreNetworkSnapshot } from "../coreNetwork"
import type { CoreNtpSnapshot } from "../coreNtp"
import type { CoreDiagnosticsSnapshot } from "../coreDiagnostics"
import type { CoreProvisionSnapshot } from "../coreProvision"
import type { SequenceRuntimeState } from "../sequences"
// ---------------------------------------------------------------------
// WS channels (Backend → Frontend)
// ---------------------------------------------------------------------

export enum WSChannel {
  SYSTEM_INFO    = "system/info",
  DEVICE_STATE    = "devices/state",
  DEVICE_REGISTER = "devices/register",
  DEVICE_RESP     = "devices/resp",
  DEVICE_STATUS   = "devices/status",
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
  | DeviceStateEvent
  | DeviceRegisterEvent
  | DeviceRespEvent
  | DeviceHeartbeatEvent
  | TimeStatusEvent

export type WSEvent = ChannelWSEvent | SequenceWsEvent
