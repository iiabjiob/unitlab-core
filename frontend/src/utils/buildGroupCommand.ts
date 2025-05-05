import type { Signal } from '@/types/signal'
import type { WSCommand } from '@/types/ws'

export function buildGroupCommand(unitId: string, signals: Signal[], value: boolean): WSCommand {
  return {
    action: 'set_group',
    unitId,
    actions: signals.map(signal => ({
      index: signal.index,
      state: value,
      delay_ms: signal.delayMs ?? 0,
      is_pulse: signal.isPulse ?? false,
      pulse_duration: signal.pulseDurationMs ?? 200
    }))
  }
}
