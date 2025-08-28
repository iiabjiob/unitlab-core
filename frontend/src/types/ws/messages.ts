// ---------------------------------------------------------------------
// Actions (WS → Backend)
// ---------------------------------------------------------------------
export enum WSAction {
  SUBSCRIBE = "subscribe",
  UNSUBSCRIBE = "unsubscribe",
  SET_DO_COMMAND = "set_do_command",
  SET_AO_COMMAND = "set_ao_command",
  GET_STATES = "get_states",
  SCAN_DEVICES = "scan_devices",
}

export enum CmdMode {
  SET_SINGLE_BIT = "SET_SINGLE_BIT",
  SET_ALL_BIT = "SET_ALL_BIT",
  SET_PAIR_BIT = "SET_PAIR_BIT",
  SET_SINGLE_FLOAT = "SET_SINGLE_FLOAT",
}

export enum ReqStateMode {
  REQ_SINGLE_BIT = "REQ_SINGLE_BIT",
  REQ_ALL_BIT = "REQ_ALL_BIT",
  REQ_SINGLE_FLOAT = "REQ_SINGLE_FLOAT",
  REQ_ALL_FLOAT = "REQ_ALL_FLOAT",
}

// ---------------------------------------------------------------------
// Subscribe / Unsubscribe
// ---------------------------------------------------------------------
export interface WsSubscribeMessage {
  action: WSAction.SUBSCRIBE
  channels: string[]
}
export interface WsUnsubscribeMessage {
  action: WSAction.UNSUBSCRIBE
  channels: string[]
}

// ---------------------------------------------------------------------
// Device management
// ---------------------------------------------------------------------
export interface ScanDevicesMessage {
  action: WSAction.SCAN_DEVICES
}

export interface RequestStateMessage {
  action: WSAction.GET_STATES
  unit_id: string
  device_type: "do" | "di" | "ao"
  mode: ReqStateMode
  ch?: number
}

// ---------------------------------------------------------------------
// Commands
// ---------------------------------------------------------------------
export interface SetDoCommandMessage {
  action: WSAction.SET_DO_COMMAND
  unit_id: string
  mode: CmdMode
  ch?: number
  value?: number
  bitmask?: number
  chA?: number
  chB?: number
  state2b?: number
}

export interface SetAoCommandMessage {
  action: WSAction.SET_AO_COMMAND
  unit_id: string
  ch: number
  value: number
}

// ---------------------------------------------------------------------
// Union
// ---------------------------------------------------------------------
export type WSMessage =
  | WsSubscribeMessage
  | WsUnsubscribeMessage
  | ScanDevicesMessage
  | RequestStateMessage
  | SetDoCommandMessage
  | SetAoCommandMessage
