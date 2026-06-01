/// <reference lib="webworker" />

import { parseScdSource } from "@/modules/scd-sld-core"
import {
  buildIec61850DebugDocument,
  type Iec61850DebugDocument,
} from "./iec61850DebugTree"

export type Iec61850DebugWorkerRequest = {
  type: "parse"
  requestId: number
  file: File
}

export type Iec61850DebugWorkerProgressStage = "reading" | "hashing" | "parsing" | "building"

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
    document: Iec61850DebugDocument
    parseDurationMs: number
    treeBuildDurationMs: number
  }
  | {
    type: "error"
    requestId: number
    message: string
  }

const worker = self as DedicatedWorkerGlobalScope
const DEBUG_TREE_SIGNAL_ROW_LIMIT = 10_000
const DEBUG_TREE_TOTAL_SIGNAL_ROW_LIMIT = 50_000
const DEBUG_DETAIL_ROW_LIMIT = 10_000
const DEBUG_DIAGNOSTIC_ROW_LIMIT = Number.POSITIVE_INFINITY

worker.onmessage = (event: MessageEvent<Iec61850DebugWorkerRequest>) => {
  const message = event.data
  if (message.type !== "parse") return

  void parseInWorker(message)
}

async function parseInWorker(message: Iec61850DebugWorkerRequest) {
  try {
    postProgress(message.requestId, "hashing", "Hashing SCD")
    const contentHash = await hashFile(message.file)

    postProgress(message.requestId, "reading", "Reading SCD")
    const xmlText = await message.file.text()

    postProgress(message.requestId, "parsing", "Parsing SCD")
    const parseStartedAt = performance.now()
    const model = parseScdSource({
      fileName: message.file.name,
      contentHash,
      xmlText,
    })
    const parseDurationMs = Math.round(performance.now() - parseStartedAt)

    postProgress(message.requestId, "building", "Building debug tree")
    const treeBuildStartedAt = performance.now()
    const document = buildIec61850DebugDocument(model, {
      maxSignalRowsPerCollection: DEBUG_TREE_SIGNAL_ROW_LIMIT,
      maxTotalSignalRows: DEBUG_TREE_TOTAL_SIGNAL_ROW_LIMIT,
      maxDetailRowsPerSection: DEBUG_DETAIL_ROW_LIMIT,
      maxDiagnostics: DEBUG_DIAGNOSTIC_ROW_LIMIT,
    })

    worker.postMessage({
      type: "result",
      requestId: message.requestId,
      contentHash,
      document,
      parseDurationMs,
      treeBuildDurationMs: Math.round(performance.now() - treeBuildStartedAt),
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

async function hashFile(file: File): Promise<string> {
  if (globalThis.crypto?.subtle) {
    const bytes = await file.arrayBuffer()
    const digest = await globalThis.crypto.subtle.digest("SHA-256", bytes)
    return Array.from(new Uint8Array(digest))
      .map(byte => byte.toString(16).padStart(2, "0"))
      .join("")
  }

  let hash = 0xcbf29ce484222325n
  const prime = 0x100000001b3n
  const mask = 0xffffffffffffffffn
  const reader = file.stream().getReader()

  try {
    while (true) {
      const result = await reader.read()
      if (result.done) break
      for (const byte of result.value) {
        hash ^= BigInt(byte)
        hash = (hash * prime) & mask
      }
    }
  } finally {
    reader.releaseLock()
  }

  return `fnv1a64-${file.size}-${hash.toString(16).padStart(16, "0")}`
}
