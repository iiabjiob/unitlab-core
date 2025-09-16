import {
  WSAction,
  CmdMode,
  type WSMessage,
  type SetDoCommandMessage,
  type SetAoCommandMessage,
} from "@/types/ws/messages"
import { StepKind, type SequenceStep } from "@/types/sequences"

// ---------------------------------------------------------------------
// Describe step (for UI)
// ---------------------------------------------------------------------
export function describeStep(step: SequenceStep): string {
  switch (step.kind) {
    case StepKind.WAIT:
      return `Wait ${step.payload?.ms ?? 0} ms`

    case StepKind.DO_RESET_ALL:
      return `DO: reset all (unit: ${step.unit_id ?? "?"})`

    case "DO_SET": {
      const { mode, ch, value, bitmask } = step.payload ?? {}
      if (mode === CmdMode.SET_SINGLE_BIT) {
        return `DO: set channel ${ch} = ${value} (unit: ${step.unit_id ?? "?"})`
      }
      if (mode === CmdMode.SET_ALL_BIT) {
        return `DO: set bitmask 0x${(bitmask ?? 0)
          .toString(16)
          .toUpperCase()} (unit: ${step.unit_id ?? "?"})`
      }
      if (mode === CmdMode.SET_SINGLE_FLOAT) {
        return `AO: set channel ${ch} = ${value} (unit: ${step.unit_id ?? "?"})`
      }
      return `DO: command (mode=0x${(mode ?? 0).toString(16).toUpperCase()})`
    }

    case StepKind.AO_SET: {
      const { ch, value } = step.payload ?? {}
      return `AO: set channel ${ch} = ${value} (unit: ${step.unit_id ?? "?"})`
    }

    default:
      return `❓ Unknown step kind: ${step.kind}`
  }
}

// ---------------------------------------------------------------------
// Map step → WSMessage
// ---------------------------------------------------------------------
export function toWSMessage(step: SequenceStep): WSMessage | null {
  switch (step.kind) {
    case StepKind.WAIT:
      return null // handled locally

    case StepKind.DO_SET: {
      const msg: SetDoCommandMessage = {
        action: WSAction.SET_DO_COMMAND,
        unit_id: step.unit_id ?? "",
        mode: step.payload?.mode ?? CmdMode.SET_SINGLE_BIT,
        ch: step.payload?.ch,
        value: step.payload?.value,
        bitmask: step.payload?.bitmask,
        chA: step.payload?.chA,
        chB: step.payload?.chB,
        state2b: step.payload?.state2b,
        pulse_ms: step.payload?.pulse_ms,
      }
      return msg
    }

    case StepKind.DO_RESET_ALL: {
      const msg: SetDoCommandMessage = {
        action: WSAction.SET_DO_COMMAND,
        unit_id: step.unit_id ?? "",
        mode: CmdMode.SET_ALL_BIT,
        bitmask: 0,
      }
      return msg
    }

    case StepKind.AO_SET: {
      const msg: SetAoCommandMessage = {
        action: WSAction.SET_AO_COMMAND,
        unit_id: step.unit_id ?? "",
        ch: step.payload?.ch ?? 0,
        value: step.payload?.value ?? 0,
      }
      return msg
    }

    default:
      return null
  }
}
