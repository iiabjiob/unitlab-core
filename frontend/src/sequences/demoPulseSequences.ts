import { CmdMode } from "@/types/ws/messages"
import type { SequenceDef } from "@/types/sequences"

// --- Helpers ---
const DO_SINGLE = (unit_id: string, ch: number, value: 0 | 1) =>
  ({ unit_id, mode: CmdMode.SET_SINGLE_BIT, ch, value } as const)

const DO_MASK = (unit_id: string, bitmask: number) =>
  ({ unit_id, mode: CmdMode.SET_ALL_BIT, bitmask } as const)

const DO_PULSE = (unit_id: string, ch: number, value: 0 | 1, pulse_ms: number) =>
  ({ unit_id, mode: CmdMode.SET_PULSE_BIT, ch, value, pulse_ms } as const)


// --- Sequence definition ---
export function buildPulseDemoSequence(unit_id: string): SequenceDef {
  return {
    id: `pulse-demo-${unit_id}`,
    name: "Pilot Demo – Pulse / Latch / Mask",
    description: "Demonstrates pulse commands, one latch, and full-channel mask on/off.",
    steps: [
      { kind: "DO_RESET_ALL", unit_id },
      { kind: "WAIT", ms: 500 },

      // Four pulses: channels 0..3 with different durations
      { kind: "DO_SET", cmd: DO_PULSE(unit_id, 0, 1, 3000) },
      { kind: "WAIT", ms: 100 }, // wait a bit between pulses
      { kind: "DO_SET", cmd: DO_PULSE(unit_id, 1, 1, 1000) },
      { kind: "WAIT", ms: 100 },
      { kind: "DO_SET", cmd: DO_PULSE(unit_id, 2, 1, 500) },
      { kind: "WAIT", ms: 100 },
      { kind: "DO_SET", cmd: DO_PULSE(unit_id, 3, 1, 800) },
      { kind: "WAIT", ms: 1000 },

      // One latch: channel 4 stays ON
      { kind: "DO_SET", cmd: DO_SINGLE(unit_id, 4, 1) },
      { kind: "WAIT", ms: 1000 },

      // Bitmask: turn ON all channels (assuming first 8 channels)
      { kind: "DO_SET", cmd: DO_MASK(unit_id, 0xFF) },
      { kind: "WAIT", ms: 1000 },

      // Reset all channels
      { kind: "DO_SET", cmd: DO_MASK(unit_id, 0x00) },
    ],
  }
}
