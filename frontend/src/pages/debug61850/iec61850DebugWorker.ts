/// <reference lib="webworker" />

import { parseScdSource, type NormalizedSclModel } from "@/modules/scd-sld-core"

export type Iec61850DebugWorkerRequest = {
  type: "parse"
  requestId: number
  fileName: string
  xmlText: string
}

export type Iec61850DebugWorkerProgressStage = "hashing" | "parsing"

export type Iec61850DebugWorkerResponse =
  | {
    type: "progress"
    requestId: number
    stage: Iec61850DebugWorkerProgressStage
    label: string
  }
  | {
    type: "result"
    requestId: number
    contentHash: string
    model: NormalizedSclModel
    parseDurationMs: number
  }
  | {
    type: "error"
    requestId: number
    message: string
  }

const worker = self as DedicatedWorkerGlobalScope

worker.onmessage = (event: MessageEvent<Iec61850DebugWorkerRequest>) => {
  const message = event.data
  if (message.type !== "parse") return

  void parseInWorker(message)
}

async function parseInWorker(message: Iec61850DebugWorkerRequest) {
  try {
    postProgress(message.requestId, "hashing", "Hashing SCD")
    const contentHash = await hashText(message.xmlText)

    postProgress(message.requestId, "parsing", "Parsing SCD")
    const parseStartedAt = performance.now()
    const model = parseScdSource({
      fileName: message.fileName,
      contentHash,
      xmlText: message.xmlText,
    })

    worker.postMessage({
      type: "result",
      requestId: message.requestId,
      contentHash,
      model,
      parseDurationMs: Math.round(performance.now() - parseStartedAt),
    } satisfies Iec61850DebugWorkerResponse)
  } catch (error) {
    worker.postMessage({
      type: "error",
      requestId: message.requestId,
      message: error instanceof Error ? error.message : "Failed to parse SCD",
    } satisfies Iec61850DebugWorkerResponse)
  }
}

function postProgress(
  requestId: number,
  stage: Iec61850DebugWorkerProgressStage,
  label: string,
) {
  worker.postMessage({
    type: "progress",
    requestId,
    stage,
    label,
  } satisfies Iec61850DebugWorkerResponse)
}

async function hashText(value: string): Promise<string> {
  if (globalThis.crypto?.subtle) {
    const bytes = new TextEncoder().encode(value)
    const digest = await globalThis.crypto.subtle.digest("SHA-256", bytes)
    return Array.from(new Uint8Array(digest))
      .map(byte => byte.toString(16).padStart(2, "0"))
      .join("")
  }

  let hash = 0
  for (let index = 0; index < value.length; index += 1) {
    hash = ((hash << 5) - hash + value.charCodeAt(index)) | 0
  }
  return `fallback-${value.length}-${Math.abs(hash)}`
}
