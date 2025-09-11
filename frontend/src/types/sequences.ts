import type { WSMessage } from "@/types/ws/messages" // adjust path if needed
import { WSAction, CmdMode } from "@/types/ws/messages"


// A single step in a sequence.
export type SequenceStep =
  | { kind: "DO_SET"; cmd: Omit<Extract<WSMessage, { action: typeof WSAction.SET_DO_COMMAND }>, "action"> }
  | { kind: "WAIT"; ms: number }
  | { kind: "DO_RESET_ALL"; unit_id: string }


export interface SequenceDef {
  id: string
  name: string
  description?: string
  steps: SequenceStep[]
}


// Utility to render a human-friendly label for a step
export function describeStep(step: SequenceStep): string {
  switch (step.kind) {
    case "WAIT":
      return `Wait ${step.ms} ms`
    case "DO_RESET_ALL":
      return `DO: reset all (unit: ${step.unit_id})`
    case "DO_SET": {
      const { unit_id, mode, ch, value, bitmask } = step.cmd
      if (mode === CmdMode.SET_SINGLE_BIT) return `DO: set channel ${ch} = ${value} (unit: ${unit_id})`
      if (mode === CmdMode.SET_ALL_BIT) return `DO: set bitmask 0x${(bitmask ?? 0).toString(16).toUpperCase()} (unit: ${unit_id})`
      if (mode === CmdMode.SET_SINGLE_FLOAT) return `AO: set channel ${ch} = ${value} (unit: ${unit_id})`
      return `DO: command (mode=0x${mode.toString(16).toUpperCase()})`
    }
  }
}


// Convert a DO_RESET_ALL step into an actual WS DO command.
export function toResetAllCmd(unit_id: string) {
  return {
    action: WSAction.SET_DO_COMMAND as const,
    unit_id,
    mode: CmdMode.SET_ALL_BIT,
    bitmask: 0,
  }
}
