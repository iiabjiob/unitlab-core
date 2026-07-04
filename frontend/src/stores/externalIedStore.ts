import { computed, ref } from "vue"
import { defineStore } from "pinia"

import { VerificationAPI } from "@/api/verification.api"
import { useWorkspaceStore } from "@/stores/workspaceStore"
import type {
  ExternalIedStatus as WsExternalIedStatus,
  ExternalIedStatusChangedEvent,
  ExternalIedStatusRecord,
  ExternalIedStatusSnapshotEvent,
} from "@/types/ws/events"

export type ExternalIedStatus = WsExternalIedStatus

export interface ExternalIedTarget {
  ip: string
  port?: number
  signalIds: number[]
}

export interface ExternalIedRecord {
  ip: string
  port: number
  status: ExternalIedStatus
  signalIds: number[]
  lastCheckedAt: string | null
  lastError: string | null
  checkKind: "none" | "tcp_connect"
  failureCode: "unreachable" | "mms_unavailable" | "network_unreachable" | "probe_failed" | null
}

const VALID_STATUSES = new Set<ExternalIedStatus>(["not_applicable", "unknown", "expected", "reachable", "offline"])

function normalizeIp(value: unknown): string | null {
  const text = String(value ?? "").trim()
  if (!/^\d{1,3}(?:\.\d{1,3}){3}$/.test(text)) return null
  const octets = text.split(".").map(part => Number.parseInt(part, 10))
  if (octets.some(octet => !Number.isInteger(octet) || octet < 0 || octet > 255)) return null
  return octets.join(".")
}

function normalizeSignalIds(signalIds: readonly unknown[]): number[] {
  const seen = new Set<number>()
  const normalized: number[] = []
  signalIds.forEach((raw) => {
    const signalId = Number(raw)
    if (!Number.isFinite(signalId) || signalId <= 0 || seen.has(signalId)) return
    seen.add(signalId)
    normalized.push(signalId)
  })
  return normalized.sort((left, right) => left - right)
}

function normalizePort(value: unknown): number {
  const port = Number(value)
  return Number.isInteger(port) && port >= 1 && port <= 65535 ? port : 102
}

function endpointKey(ip: string, port: number): string {
  return `${ip}:${port}`
}

function normalizeRecord(record: ExternalIedStatusRecord): ExternalIedRecord | null {
  const ip = normalizeIp(record.ip)
  if (!ip) return null
  const status = VALID_STATUSES.has(record.status) ? record.status : "expected"
  return {
    ip,
    port: normalizePort(record.port),
    status,
    signalIds: normalizeSignalIds(record.signal_ids ?? []),
    lastCheckedAt: record.last_checked_at ?? null,
    lastError: record.last_error ?? null,
    checkKind: record.check_kind === "tcp_connect" ? "tcp_connect" : "none",
    failureCode: record.failure_code ?? null,
  }
}

function targetSignature(workspaceId: number | null | undefined, targets: readonly ExternalIedTarget[]): string {
  const normalizedTargets = targets
    .map(target => ({
      ip: normalizeIp(target.ip),
      port: normalizePort(target.port),
      signalIds: normalizeSignalIds(target.signalIds),
    }))
    .filter((target): target is { ip: string; port: number; signalIds: number[] } => Boolean(target.ip && target.signalIds.length))
    .sort((left, right) => left.ip.localeCompare(right.ip, undefined, { numeric: true }) || left.port - right.port)
  return JSON.stringify({
    workspaceId: Number(workspaceId) || null,
    targets: normalizedTargets,
  })
}

export const useExternalIedStore = defineStore("externalIedStore", () => {
  const workspaceStore = useWorkspaceStore()
  const records = ref<Record<string, ExternalIedRecord>>({})
  const configuredTargets = ref<Record<string, ExternalIedTarget>>({})
  const statusRevision = ref(0)
  const lastChangedIps = ref<string[]>([])
  const lastChangedSignalIds = ref<number[]>([])
  const lastError = ref<string | null>(null)
  let lastConfiguredSignature: string | null = null
  let configuredWorkspaceId: number | null = null

  function publishChanged(changedIps: string[], changedSignalIds: number[]) {
    lastChangedIps.value = Array.from(new Set(changedIps))
    lastChangedSignalIds.value = Array.from(new Set(changedSignalIds))
    statusRevision.value += 1
  }

  async function configureExpectedDevices(workspaceId: number | null | undefined, targets: readonly ExternalIedTarget[]) {
    const signature = targetSignature(workspaceId, targets)
    if (signature === lastConfiguredSignature) {
      return
    }
    lastConfiguredSignature = signature

    const normalizedWorkspaceId = Number(workspaceId)
    const normalizedTargets = targets
      .map(target => ({
        ip: normalizeIp(target.ip),
        port: normalizePort(target.port),
        signalIds: normalizeSignalIds(target.signalIds),
      }))
      .filter((target): target is { ip: string; port: number; signalIds: number[] } => Boolean(target.ip && target.signalIds.length))

    if (!Number.isFinite(normalizedWorkspaceId) || normalizedWorkspaceId <= 0) {
      clearLocal()
      configuredWorkspaceId = null
      return
    }

    if (configuredWorkspaceId !== null && configuredWorkspaceId !== normalizedWorkspaceId) {
      clearLocal()
    }
    configuredWorkspaceId = normalizedWorkspaceId

    const previousSignalIds = Object.values(configuredTargets.value).flatMap(target => target.signalIds)
    configuredTargets.value = Object.fromEntries(
      normalizedTargets.map(target => [endpointKey(target.ip, target.port), { ip: target.ip, port: target.port, signalIds: target.signalIds }]),
    )

    if (normalizedTargets.length === 0) {
      clearLocal(previousSignalIds)
    } else {
      const nextRecords = { ...records.value }
      normalizedTargets.forEach((target) => {
        const key = endpointKey(target.ip, target.port)
        nextRecords[key] = nextRecords[key] ?? {
          ip: target.ip,
          port: target.port,
          status: "expected",
          signalIds: target.signalIds,
          lastCheckedAt: null,
          lastError: null,
          checkKind: "none",
          failureCode: null,
        }
        nextRecords[key].signalIds = target.signalIds
      })
      records.value = Object.fromEntries(Object.entries(nextRecords).filter(([key]) => Boolean(configuredTargets.value[key])))
      publishChanged(normalizedTargets.map(target => target.ip), [...previousSignalIds, ...normalizedTargets.flatMap(target => target.signalIds)])
    }

    try {
      await VerificationAPI.configureExternalIedTargets(normalizedWorkspaceId, {
        targets: normalizedTargets.map(target => ({
          ip: target.ip,
          port: target.port,
          signal_ids: target.signalIds,
        })),
      })
      lastError.value = null
    } catch (err) {
      lastError.value = err instanceof Error ? err.message : String(err)
      lastConfiguredSignature = null
    }
  }

  function applySnapshot(event: ExternalIedStatusSnapshotEvent) {
    if (Number(event.workspace_id) !== Number(workspaceStore.activeWorkspaceId)) {
      return
    }
    const nextRecords: Record<string, ExternalIedRecord> = {}
    const changedIps: string[] = []
    const changedSignalIds = [...normalizeSignalIds(event.removed_signal_ids ?? [])]
    event.devices.forEach((rawRecord) => {
      const record = normalizeRecord(rawRecord)
      if (!record) return
      const key = endpointKey(record.ip, record.port)
      const previous = records.value[key]
      nextRecords[key] = record
      if (JSON.stringify(previous ?? null) !== JSON.stringify(record)) {
        changedIps.push(record.ip)
        changedSignalIds.push(...record.signalIds)
      }
    })
    Object.entries(records.value).forEach(([ip, record]) => {
      if (nextRecords[ip]) return
      changedIps.push(ip)
      changedSignalIds.push(...record.signalIds)
    })
    records.value = nextRecords
    publishChanged(changedIps, changedSignalIds)
  }

  function applyStatusChanged(event: ExternalIedStatusChangedEvent) {
    if (Number(event.workspace_id) !== Number(workspaceStore.activeWorkspaceId)) {
      return
    }
    const ip = normalizeIp(event.ip)
    if (!ip) return
    const port = normalizePort(event.port)
    const key = endpointKey(ip, port)
    const signalIds = normalizeSignalIds(event.signal_ids ?? [])
    const status = VALID_STATUSES.has(event.new_status) ? event.new_status : "unknown"
    const previous = records.value[key]
    if (previous?.status === status && previous.lastCheckedAt === event.checked_at) {
      return
    }
    records.value = {
      ...records.value,
      [key]: {
        ip,
        port,
        status,
        signalIds,
        lastCheckedAt: event.checked_at,
        lastError: event.error ?? null,
        checkKind: event.check_kind === "tcp_connect" ? "tcp_connect" : "none",
        failureCode: event.failure_code ?? null,
      },
    }
    publishChanged([ip], signalIds)
  }

  function clearLocal(signalIds: readonly number[] = Object.values(records.value).flatMap(record => record.signalIds)) {
    records.value = {}
    configuredTargets.value = {}
    publishChanged([], normalizeSignalIds(signalIds))
  }

  function getStatus(ip: string | null | undefined, port?: number | null): ExternalIedStatus {
    const normalizedIp = normalizeIp(ip)
    if (!normalizedIp) return "not_applicable"
    return records.value[endpointKey(normalizedIp, normalizePort(port))]?.status ?? "not_applicable"
  }

  const active = computed(() => Object.keys(configuredTargets.value).length > 0 || Object.keys(records.value).length > 0)
  const devices = computed(() => Object.values(records.value))

  return {
    active,
    records,
    devices,
    statusRevision,
    lastChangedIps,
    lastChangedSignalIds,
    lastError,
    configureExpectedDevices,
    applySnapshot,
    applyStatusChanged,
    getStatus,
    clearLocal,
  }
})
