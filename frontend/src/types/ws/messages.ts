// ---------------------------------------------------------------------
// Actions (WS → Backend)
// ---------------------------------------------------------------------
export enum WSAction {
  SET_DO_COMMAND = "set_do_command",
  SET_AO_COMMAND = "set_ao_command",
  GET_STATES     = "get_states",
  SCAN_DEVICES   = "scan_devices",
}

export enum ReqStateMode {
  REQ_SINGLE_BIT   = 0x10,
  REQ_ALL_BIT      = 0x11,
  REQ_SINGLE_FLOAT = 0x14,
  REQ_ALL_FLOAT    = 0x15,
}

export enum CmdMode {
  SET_SINGLE_BIT   = 0x20,
  SET_ALL_BIT      = 0x21,
  SET_PAIR_BIT     = 0x22,
  SET_PULSE_BIT    = 0x23,
  SET_SINGLE_FLOAT = 0x30,
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
  type: "do" | "di" | "ao"
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

  // common for SET_SINGLE_BIT / SET_PULSE_BIT
  ch?: number
  value?: number

  // only for SET_ALL_BIT
  bitmask?: number

  // only for SET_PAIR_BIT
  chA?: number
  chB?: number
  state2b?: number

  // only for SET_PULSE_BIT
  pulse_ms?: number
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
  | ScanDevicesMessage
  | RequestStateMessage
  | SetDoCommandMessage
  | SetAoCommandMessage
