export type WSCommand =
  | { action: 'subscribe'; channels: string[] }
  | { action: 'unsubscribe'; channels: string[] }
  | { action: 'set_pin'; unitId: string; index: number; value: boolean; delay_ms: number; is_pulse: boolean; pulse_duration: number }
  | { action: 'set_group'; unitId: string; actions: WSGroupAction[] }
  | { action: 'scan_devices' }
  | { action: 'request_states'; unitId: string }


export interface WSGroupAction {
  index: number
  state: boolean
  delay_ms: number
  is_pulse: boolean
  pulse_duration: number
}

export type WSAction = WSCommand['action']
