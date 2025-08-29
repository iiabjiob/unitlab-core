import type { TimeStatus } from "../time"
// ---------------------------------------------------------------------
// Каналы WS (Backend → Frontend)
// ---------------------------------------------------------------------

export enum WSChannel {
  DEVICE_STATE    = "devices/state",
  DEVICE_REGISTER = "devices/register",
  DEVICE_RESP     = "devices/resp",
  DEVICE_STATUS   = "devices/status",
  TIME_STATUS     = "time/status",
}

// ---------------------------------------------------------------------
// Протокол (Modes, Cmd, State) — зеркалит backend/protocol/modes.py
// ---------------------------------------------------------------------
export enum StateMode {
  STATE_SINGLE_BIT    = "STATE_SINGLE_BIT",
  STATE_ALL_BIT       = "STATE_ALL_BIT",
  STATE_SINGLE_FLOAT  = "STATE_SINGLE_FLOAT",
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
}

// ---------------------------------------------------------------------
// Device Events
// ---------------------------------------------------------------------

export interface DeviceStateEvent {
  channel: WSChannel.DEVICE_STATE
  unit_id: string
  device_type: string
  timestamp: number
  mode: StateMode
  payload: Record<string, any>
}

export interface DeviceRegisterEvent {
  channel: WSChannel.DEVICE_REGISTER
  unit_id: string
  device_type: string
  channels: number
  firmware_version: number
  is_active: boolean
  status: "online" | "offline"
  last_seen?: number
}

export interface DeviceRespEvent {
  channel: WSChannel.DEVICE_RESP
  unit_id: string
  device_type: string
  packet_id: number
  status: RespStatus
  error: RespError
  timestamp: number
}

export interface DeviceHeartbeatEvent {
  channel: WSChannel.DEVICE_STATUS
  unit_id: string
  device_type: string
  status: "online" | "offline"
  last_seen: number
}

export interface TimeStatusEvent extends TimeStatus {
  channel: WSChannel.TIME_STATUS
}

export type WSEvent =
  | DeviceStateEvent
  | DeviceRegisterEvent
  | DeviceRespEvent
  | DeviceHeartbeatEvent
  | TimeStatusEvent
