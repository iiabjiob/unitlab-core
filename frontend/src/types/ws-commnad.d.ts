export type WSCommand =
  | { action: 'subscribe'; channels: string[] }
  | { action: 'unsubscribe'; channels: string[] }
  | { action: 'set_pin'; unitId: string; index: number; value: boolean }
  | { action: 'scan_devices' }
  | { action: 'request_states'; unitId: string }

// Если нужно, можно также сделать enum
export type WSAction = WSCommand['action']
