export type WSCommand =
  | { action: 'subscribe'; channels: string[] }
  | { action: 'unsubscribe'; channels: string[] }
  | { action: 'set_do'; unitId: string; command: WSDOCommand }
  | { action: 'set_do_group'; unitId: string; commands: WSGroupAction[] }
  | { action: 'set_ao'; unitId: string; command: WSAOCommand }
  | { action: 'scan_devices' }
  | { action: 'request_states'; unitId: string }

export interface WSDOCommand {
  index: number
  state: boolean
  delayBeforeMs?: number
  isPulse?: boolean
  pulseMs?: number
}

export interface WSGroupAction {
  index: number
  state: boolean
  delayBeforeMs?: number
  isPulse?: boolean
  pulseMs?: number
}

export interface WSAOCommand {
  index: number
  value: number          // mA или нормализованное значение (0–100%)
  minValue?: number      // для UI/валидации
  maxValue?: number
}
