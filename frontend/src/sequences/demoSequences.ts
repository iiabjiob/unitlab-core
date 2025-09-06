import { CmdMode } from "@/types/ws/messages"
import type { SequenceDef } from "@/types/sequences"


// Helper builders
const DO_SINGLE = (unit_id: string, ch: number, value: 0 | 1) => ({ unit_id, mode: CmdMode.SET_SINGLE_BIT, ch, value } as const)
const DO_MASK = (unit_id: string, bitmask: number) => ({ unit_id, mode: CmdMode.SET_ALL_BIT, bitmask } as const)


export function buildPilotSequence(): SequenceDef {

  let unit_id = "9458EC";

  // Channels assumed 0-based; adjust to your hardware mapping as needed.
  return {
    id: `pilot-basic-${unit_id}`,
    name: "Pilot Demo – Basic DO Sequence",
    description: "Shows single-bit set/reset, group bitmask, and a simulated pulse via WAIT.",
    steps: [
      { kind: "DO_RESET_ALL", unit_id },
      { kind: "WAIT", ms: 500 },


      // Single-bit ON → wait → OFF
      { kind: "DO_SET", cmd: DO_SINGLE(unit_id, 0, 1) },
      { kind: "WAIT", ms: 1000 },
      { kind: "DO_SET", cmd: DO_SINGLE(unit_id, 0, 0) },
      { kind: "WAIT", ms: 300 },


      // Group ON (e.g., channels 0,2,4) → wait → group OFF
      { kind: "DO_SET", cmd: DO_MASK(unit_id, 0b00010101) }, // 0,2,4 ON
      { kind: "WAIT", ms: 800 },
      { kind: "DO_SET", cmd: DO_MASK(unit_id, 0) }, // all OFF
      { kind: "WAIT", ms: 300 },


      // Simulated pulse on channel 3 (ON 300 ms → OFF)
      { kind: "DO_SET", cmd: DO_SINGLE(unit_id, 3, 1) },
      { kind: "WAIT", ms: 300 },
      { kind: "DO_SET", cmd: DO_SINGLE(unit_id, 3, 0) },
      { kind: "WAIT", ms: 300 },


      // Walk through channels 0..3 with short delays
      { kind: "DO_SET", cmd: DO_SINGLE(unit_id, 0, 1) },
      { kind: "WAIT", ms: 200 },
      { kind: "DO_SET", cmd: DO_SINGLE(unit_id, 1, 1) },
      { kind: "WAIT", ms: 200 },
      { kind: "DO_SET", cmd: DO_SINGLE(unit_id, 2, 1) },
      { kind: "WAIT", ms: 200 },
      { kind: "DO_SET", cmd: DO_SINGLE(unit_id, 3, 1) },
      { kind: "WAIT", ms: 400 },
      { kind: "DO_SET", cmd: DO_MASK(unit_id, 0) },
    ],
  }
}
