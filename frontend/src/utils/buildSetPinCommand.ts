import type { Signal } from '@/types/signal'
import { WSCommand } from '@/types/ws'

export function buildSetPinCommand(signal: Signal, unitId: string, value: boolean): WSCommand {
  return {
    action: 'set_pin',
    unitId,
    index: signal.index,
    value,
    delay_ms: signal.delayMs ?? 0,
    is_pulse: signal.isPulse ?? false,
    pulse_duration: signal.pulseDurationMs ?? 200
  }
}
