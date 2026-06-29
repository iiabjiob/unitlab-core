import { extractSourceRowFromSignalMetadata } from "./sourceColumns"
import type { SignalAllocationRow } from "@/types/signal"

export type Online61850PreparationTarget = {
  key: string
  host: string
  port: number
  iedName: string | null
  accessPointName: string | null
  signalIds: number[]
  signalLabels: string[]
  sourceAddresses: string[]
}

type Online61850PreparationTargetSource = {
  signalId: number
  signalLabel: string
  host: string
  port: number
  iedName: string | null
  accessPointName: string | null
  sourceAddress: string | null
}

const TRANSPORT_HOST_KEYS = [
  "transport_host",
  "transport_reference",
  "host",
  "ip_address",
  "device_ip",
  "target_ip",
  "mms_host",
  "endpoint_host",
  "ip",
]

const IEC61850_ADDRESS_KEYS = [
  "iec61850_address",
  "iec61850",
  "iec61850_path",
  "mms_reference",
  "object_reference",
  "signal_path",
  "reference",
]

const IED_NAME_KEYS = [
  "ied_name",
  "iedname",
  "endpoint_ied_name",
  "device_name",
  "server_name",
]

const ACCESS_POINT_KEYS = [
  "access_point_name",
  "accesspointname",
  "ap_name",
]

export function buildOnline61850PreparationTargets(rows: readonly SignalAllocationRow[]): {
  targets: Online61850PreparationTarget[]
  skippedRows: Array<{ signalId: number; signalLabel: string; reason: string }>
} {
  const sources: Online61850PreparationTargetSource[] = []
  const skippedRows: Array<{ signalId: number; signalLabel: string; reason: string }> = []

  rows.forEach((row) => {
    const signalId = Number(row.signal_id)
    if (!Number.isFinite(signalId) || signalId <= 0) {
      return
    }

    const signalLabel = String(row.signal_name ?? row.signal_key ?? "").trim() || `Signal ${signalId}`
    const source = resolveOnline61850PreparationTargetSource(row, signalLabel)
    if (!source) {
      skippedRows.push({
        signalId,
        signalLabel,
        reason: "No transport host was found for the selected row.",
      })
      return
    }

    sources.push(source)
  })

  return {
    targets: groupOnline61850PreparationTargets(sources),
    skippedRows,
  }
}

function resolveOnline61850PreparationTargetSource(
  row: SignalAllocationRow,
  signalLabel: string,
): Online61850PreparationTargetSource | null {
  const sourceRow = extractSourceRowFromSignalMetadata(row.signal_metadata)
  const verificationMeta = resolveRecordCandidate(row.signal_metadata && typeof row.signal_metadata === "object" && !Array.isArray(row.signal_metadata)
    ? (row.signal_metadata as Record<string, unknown>).verification
    : null)

  const hostCandidate = firstStringValue(
    sourceRow,
    verificationMeta,
    row.signal_metadata,
    TRANSPORT_HOST_KEYS,
  )
  const normalizedHost = normalizeTransportHost(hostCandidate)
  if (!normalizedHost) {
    return null
  }

  const addressCandidate = firstStringValue(
    sourceRow,
    verificationMeta,
    row.signal_metadata,
    IEC61850_ADDRESS_KEYS,
  )
  const iedCandidate = firstStringValue(
    sourceRow,
    verificationMeta,
    row.signal_metadata,
    IED_NAME_KEYS,
  )
  const accessPointCandidate = firstStringValue(
    sourceRow,
    verificationMeta,
    row.signal_metadata,
    ACCESS_POINT_KEYS,
  )

  const parsedAddress = parseIec61850Address(addressCandidate)

  return {
    signalId: Number(row.signal_id),
    signalLabel,
    host: normalizedHost.host,
    port: normalizedHost.port,
    iedName: trimToNull(iedCandidate ?? parsedAddress.iedName),
    accessPointName: trimToNull(accessPointCandidate) ?? "AP1",
    sourceAddress: trimToNull(addressCandidate),
  }
}

function groupOnline61850PreparationTargets(
  sources: readonly Online61850PreparationTargetSource[],
): Online61850PreparationTarget[] {
  const grouped = new Map<string, Online61850PreparationTarget>()

  sources.forEach((source) => {
    const key = [
      source.host.toLowerCase(),
      String(source.port),
      String(source.iedName ?? "").toLowerCase(),
      String(source.accessPointName ?? "").toLowerCase(),
    ].join("|")

    const existing = grouped.get(key)
    if (existing) {
      existing.signalIds.push(source.signalId)
      existing.signalLabels.push(source.signalLabel)
      if (source.sourceAddress) {
        existing.sourceAddresses.push(source.sourceAddress)
      }
      return
    }

    grouped.set(key, {
      key,
      host: source.host,
      port: source.port,
      iedName: source.iedName,
      accessPointName: source.accessPointName,
      signalIds: [source.signalId],
      signalLabels: [source.signalLabel],
      sourceAddresses: source.sourceAddress ? [source.sourceAddress] : [],
    })
  })

  return [...grouped.values()].sort((left, right) => {
    const leftKey = `${left.host}:${left.port}/${left.iedName ?? ""}/${left.accessPointName ?? ""}`
    const rightKey = `${right.host}:${right.port}/${right.iedName ?? ""}/${right.accessPointName ?? ""}`
    return leftKey.localeCompare(rightKey, undefined, { numeric: true, sensitivity: "base" })
  })
}

function firstStringValue(
  ...args: Array<Record<string, unknown> | null | undefined | readonly string[]>
): string | null {
  const keyList = Array.isArray(args[args.length - 1]) ? args.pop() as readonly string[] : []
  for (const source of args as Array<Record<string, unknown> | null | undefined>) {
    if (!source || typeof source !== "object") {
      continue
    }
    const normalized = normalizeRecord(source)
    for (const key of keyList) {
      const candidate = normalized.get(canonicalKey(key))
      if (typeof candidate === "string") {
        const text = candidate.trim()
        if (text) {
          return text
        }
      }
    }
  }
  return null
}

function normalizeRecord(value: Record<string, unknown>): Map<string, unknown> {
  return new Map(Object.entries(value).map(([key, entry]) => [canonicalKey(key), entry]))
}

function canonicalKey(value: string): string {
  return String(value ?? "")
    .trim()
    .toLowerCase()
    .replace(/[^a-z0-9]+/g, "")
}

function resolveRecordCandidate(value: unknown): Record<string, unknown> | null {
  if (!value || typeof value !== "object" || Array.isArray(value)) {
    return null
  }
  return value as Record<string, unknown>
}

function trimToNull(value: string | null | undefined): string | null {
  const text = String(value ?? "").trim()
  return text ? text : null
}

function normalizeTransportHost(rawHost: string | null): { host: string; port: number } | null {
  const text = String(rawHost ?? "").trim()
  if (!text) {
    return null
  }

  let hostText = text
  let port = 102

  const firstToken = hostText.split(/\s+/, 1)[0]?.trim() ?? ""
  hostText = firstToken || hostText

  const slashIndex = hostText.indexOf("/")
  if (slashIndex > 0) {
    const suffix = hostText.slice(slashIndex + 1).trim()
    if (/^\d+$/.test(suffix) || /^\d{1,3}(?:\.\d{1,3}){3}$/.test(suffix)) {
      hostText = hostText.slice(0, slashIndex).trim()
    }
  }

  const bracketMatch = /^\[(.+)\](?::(\d{1,5}))?$/.exec(hostText)
  if (bracketMatch) {
    hostText = bracketMatch[1] ?? hostText
    if (bracketMatch[2]) {
      port = Number(bracketMatch[2])
    }
  } else {
    const colonMatch = /^(.+):(\d{1,5})$/.exec(hostText)
    if (colonMatch && colonMatch[1] && !colonMatch[1].includes(":")) {
      hostText = colonMatch[1]
      port = Number(colonMatch[2])
    }
  }

  hostText = hostText.trim()
  if (!hostText) {
    return null
  }

  if (!Number.isFinite(port) || port <= 0 || port > 65535) {
    port = 102
  }

  return {
    host: hostText,
    port,
  }
}

function parseIec61850Address(rawAddress: string | null): { iedName: string | null; accessPointName: string | null } {
  const text = String(rawAddress ?? "").trim()
  if (!text) {
    return {
      iedName: null,
      accessPointName: null,
    }
  }

  const segments = text.split("/").map(segment => segment.trim()).filter(Boolean)
  if (!segments.length) {
    return {
      iedName: null,
      accessPointName: null,
    }
  }

  return {
    iedName: segments[0] || null,
    accessPointName: null,
  }
}
