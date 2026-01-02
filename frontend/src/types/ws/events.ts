import type { Channel } from "../channel"
import type { TimeStatus } from "../time"
// ---------------------------------------------------------------------
// WS channels (Backend → Frontend)
// ---------------------------------------------------------------------

export enum WSChannel {
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
  STATE_SINGLE_FLOAT = 0x16,
  STATE_ALL_DIAG     = 0x18,
  STATE_CHANGED_BIT  = 0x19,
  STATE_DIAG_DI      = 0x1B,
  STATE_LATCHED_DI   = 0x1C,
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
}

export interface SequenceProgressEvent extends SequenceEventBase {
  event: "progress"
  step_index: number
  step_id: number
  step_type: string
  elapsed_ms: number
  completed_steps: number[]
}

export interface SequenceStepErrorEvent extends SequenceEventBase {
  event: "step_error"
  step_index: number
  message: string
}

export interface SequenceErrorEvent extends SequenceEventBase {
  event: "error"
  message: string
}

export interface SequenceStoppedEvent extends SequenceEventBase {
  event: "stopped"
}

export interface SequenceCompletedEvent extends SequenceEventBase {
  event: "completed"
  elapsed_ms: number
}

export type SequenceWsEvent =
  | SequenceStartedEvent
  | SequenceProgressEvent
  | SequenceStepErrorEvent
  | SequenceErrorEvent
  | SequenceStoppedEvent
  | SequenceCompletedEvent


export type ChannelWSEvent =
  | DeviceStateEvent
  | DeviceRegisterEvent
  | DeviceRespEvent
  | DeviceHeartbeatEvent
  | TimeStatusEvent

export type WSEvent = ChannelWSEvent | SequenceWsEvent
