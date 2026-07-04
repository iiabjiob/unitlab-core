import { extractSourceRowFromSignalMetadata } from "./sourceColumns"
import type { SignalAllocationRow } from "@/types/signal"

export type ExternalIedAvailabilityTarget = {
  ip: string
  port: number
  signalIds: number[]
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

export function isSignalRow61850VerificationEnabled(row: SignalAllocationRow): boolean {
  const metadata = row.signal_metadata
  const verificationMeta = resolveRecordCandidate(metadata && typeof metadata === "object" && !Array.isArray(metadata)
    ? (metadata as Record<string, unknown>).verification
    : null)
  return verificationMeta?.enabled === true
}

export function resolveOnline61850SignalReference(row: SignalAllocationRow): string | null {
  const sourceRow = extractSourceRowFromSignalMetadata(row.signal_metadata)
  const verificationMeta = resolveRecordCandidate(row.signal_metadata && typeof row.signal_metadata === "object" && !Array.isArray(row.signal_metadata)
    ? (row.signal_metadata as Record<string, unknown>).verification
    : null)

  return firstStringValue(
    sourceRow,
    verificationMeta,
    row.signal_metadata,
    IEC61850_ADDRESS_KEYS,
  )
}

export function buildExternalIedAvailabilityTargets(rows: readonly SignalAllocationRow[]): ExternalIedAvailabilityTarget[] {
  const signalIdsByEndpoint = new Map<string, { ip: string; port: number; signalIds: number[] }>()
  rows.forEach((row) => {
    if (!isSignalRow61850VerificationEnabled(row)) return
    if (!resolveOnline61850SignalReference(row)) return
    const host = resolveOnline61850TransportHost(row)
    const normalizedIp = normalizeIpv4Host(host?.host ?? null)
    if (!normalizedIp) return
    const signalId = Number(row.signal_id)
    if (!Number.isFinite(signalId) || signalId <= 0) return
    const port = host?.port ?? 102
    const key = `${normalizedIp}:${port}`
    const existing = signalIdsByEndpoint.get(key) ?? { ip: normalizedIp, port, signalIds: [] }
    existing.signalIds.push(signalId)
    signalIdsByEndpoint.set(key, existing)
  })

  return Array.from(signalIdsByEndpoint.values())
    .map(target => ({
      ip: target.ip,
      port: target.port,
      signalIds: Array.from(new Set(target.signalIds)).sort((left, right) => left - right),
    }))
    .sort((left, right) => (
      left.ip.localeCompare(right.ip, undefined, { numeric: true }) || left.port - right.port
    ))
}

export function resolveOnline61850TransportHost(row: SignalAllocationRow): { host: string; port: number } | null {
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
  return normalizeTransportHost(hostCandidate)
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

function normalizeIpv4Host(rawHost: string | null): string | null {
  const text = String(rawHost ?? "").trim()
  if (!/^\d{1,3}(?:\.\d{1,3}){3}$/.test(text)) return null
  const octets = text.split(".").map(part => Number.parseInt(part, 10))
  if (octets.some(octet => !Number.isInteger(octet) || octet < 0 || octet > 255)) return null
  return octets.join(".")
}
