// src/utils/sequenceUtils.ts
import type { WSMessage } from "@/types/ws/messages"
import { StepKind, type SequenceStep } from "@/types/sequences"
import type { useWebSocketStore } from "@/stores/websocketStore"
import { getLogger } from "@/utils/logger"

const logger = getLogger("SEQ")

// Run one step in sequence
export async function execStep(
  step: SequenceStep,
  ws: ReturnType<typeof useWebSocketStore>,
  toWSMessage: (step: SequenceStep) => WSMessage | null,
  describeStep: (step: SequenceStep) => string
) {
  if (step.kind === StepKind.WAIT) {
    const ms = step.payload?.ms ?? 0
    logger.debug(`⏳ Wait ${ms} ms`)
    await sleep(ms)
    return
  }

  const msg = toWSMessage(step)
  if (msg) {
    logger.debug(`➡️ Exec step: ${describeStep(step)}`, msg)
    ws.send(msg)
  } else {
    logger.warn(`❓ Unknown or non-executable step: ${step.kind}`, step)
  }
}

export function sleep(ms: number) {
  return new Promise<void>((resolve) => setTimeout(resolve, ms))
}
