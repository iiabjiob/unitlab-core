import {
  WSAction,
  CmdMode,
  type WSMessage,
} from "@/types/ws/messages"
import { StepKind, type SequenceStep } from "@/types/sequences"

import { useChannelStore } from "@/stores/channelStore"


// ---------------------------------------------------------------------
// Describe step (for UI)
// ---------------------------------------------------------------------
export function describeStep(step: SequenceStep): string {

  switch (step.kind) {
    case StepKind.WAIT:
      return `Wait ${step.payload?.ms ?? 0} ms`

    case StepKind.DO_LATCH: {
      const { ch, value } = step.payload ?? {}
      return `DO: latch channel ${ch} = ${value} (unit: ${step.unit_id ?? "?"})`
    }

    case StepKind.DO_PULSE: {
      const { ch, value, pulse_ms } = step.payload ?? {}
      return `DO: pulse channel ${ch} = ${value} for ${pulse_ms} ms (unit: ${step.unit_id ?? "?"})`
    }

    case StepKind.DO_PAIR: {
      const { chA, chB, state2b } = step.payload ?? {}
      return `DO: pair chA=${chA}, chA=${chB}, state=${state2b} (unit: ${step.unit_id ?? "?"})`
    }

    case StepKind.DO_BITMASK: {
      const { bitmask } = step.payload ?? {}
      return `DO: bitmask 0x${(bitmask ?? 0).toString(16).toUpperCase()} (unit: ${step.unit_id ?? "?"})`
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
      return null // локально

    case StepKind.DO_LATCH: {
      return {
        action: WSAction.SET_DO_COMMAND,
        unit_id: step.unit_id ?? "",
        mode: CmdMode.SET_SINGLE_BIT,
        ch: step.payload?.ch,
        value: step.payload?.value,
      }
    }

    case StepKind.DO_PULSE: {
      return {
        action: WSAction.SET_DO_COMMAND,
        unit_id: step.unit_id ?? "",
        mode: CmdMode.SET_PULSE_BIT,
        ch: step.payload?.ch,
        value: step.payload?.value,
        pulse_ms: step.payload?.pulse_ms,
      }
    }

    case StepKind.DO_PAIR: {
      return {
        action: WSAction.SET_DO_COMMAND,
        unit_id: step.unit_id ?? "",
        mode: CmdMode.SET_PAIR_BIT,
        chA: step.payload?.chA,
        chB: step.payload?.chB,
        state2b: step.payload?.state2b,
      }
    }

    case StepKind.DO_BITMASK: {
      return {
        action: WSAction.SET_DO_COMMAND,
        unit_id: step.unit_id ?? "",
        mode: CmdMode.SET_ALL_BIT,
        bitmask: step.payload?.bitmask ?? 0,
      }
    }

    case StepKind.AO_SET: {
      return {
        action: WSAction.SET_AO_COMMAND,
        unit_id: step.unit_id ?? "",
        ch: step.payload?.ch ?? 0,
        value: step.payload?.value ?? 0,
      }
    }

    default:
      return null
  }
}
