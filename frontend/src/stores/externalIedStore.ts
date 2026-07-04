import { computed, ref } from "vue"
import { defineStore } from "pinia"

import { VerificationAPI } from "@/api/verification.api"
import { useWorkspaceStore } from "@/stores/workspaceStore"
import type {
  ExternalIedStatus as WsExternalIedStatus,
  ExternalIedDiscoveryState,
  ExternalIedPlanningChangedEvent,
  ExternalIedPlanningEndpointRecord,
  ExternalIedPlanningSignalResult,
  ExternalIedPlanningSignalStatus,
  ExternalIedPlanningSnapshotEvent,
  ExternalIedPlanningState,
  ExternalIedStatusChangedEvent,
  ExternalIedStatusRecord,
  ExternalIedStatusSnapshotEvent,
} from "@/types/ws/events"
import type { VerificationExternalIedDiscoveryTreeResponse } from "@/types/verification"

export type ExternalIedStatus = WsExternalIedStatus
export type ExternalIedDiscoveryTree = VerificationExternalIedDiscoveryTreeResponse
export type { ExternalIedDiscoveryState, ExternalIedPlanningSignalStatus, ExternalIedPlanningState }

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
  discoveryState: ExternalIedDiscoveryState
  discoveryRetryAtMs: number | null
  discoveryLastError: string | null
  discoveryUpdatedAtMs: number | null
  discoveryReadyForVerification: boolean
  discoveryDeviceIdentity: string | null
  discoveryVendor: string | null
  discoveryModel: string | null
  discoveryDatasets: number | null
  discoveryRcbs: number | null
  discoveryModelSignals: number | null
  planningState: ExternalIedPlanningState
  planningUpdatedAtMs: number | null
  planningMatchedCount: number
  planningUnmatchedCount: number
  planningAmbiguousCount: number
  planningLastError: string | null
}

export interface ExternalIedPlanningCoverage {
  signalId: number
  endpoint: string
  status: ExternalIedPlanningSignalStatus
  address: string | null
  reason: string | null
  fcdaReference: string | null
  datasetReference: string | null
  rcbReference: string | null
  rcbName: string | null
}

export interface ExternalIedSummary {
  active: boolean
  ready: number
  discovering: number
  offline: number
  failed: number
}

const VALID_STATUSES = new Set<ExternalIedStatus>(["not_applicable", "unknown", "expected", "reachable", "offline"])
const VALID_PLANNING_STATES = new Set<ExternalIedPlanningState>(["NotPlanned", "Queued", "Running", "Ready", "Partial", "Failed", "Stale", "WaitingForDiscovery"])
const VALID_PLANNING_SIGNAL_STATUSES = new Set<ExternalIedPlanningSignalStatus>(["matched", "unmatched", "ambiguous", "not_planned", "stale"])
const VALID_DISCOVERY_STATES = new Set<ExternalIedDiscoveryState>([
  "NeverDiscovered",
  "Queued",
  "Running",
  "Succeeded",
  "Failed",
  "RetryWaiting",
  "Cancelled",
  "Stale",
])

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

function normalizeDiscoveryState(value: unknown): ExternalIedDiscoveryState {
  const state = String(value ?? "NeverDiscovered")
  return VALID_DISCOVERY_STATES.has(state as ExternalIedDiscoveryState)
    ? state as ExternalIedDiscoveryState
    : "NeverDiscovered"
}

function normalizeOptionalMs(value: unknown): number | null {
  const numberValue = Number(value)
  return Number.isFinite(numberValue) && numberValue >= 0 ? Math.trunc(numberValue) : null
}

function normalizeOptionalText(value: unknown): string | null {
  const text = String(value ?? "").trim()
  return text ? text : null
}

function normalizePlanningState(value: unknown): ExternalIedPlanningState {
  const state = String(value ?? "NotPlanned")
  return VALID_PLANNING_STATES.has(state as ExternalIedPlanningState)
    ? state as ExternalIedPlanningState
    : "NotPlanned"
}

function normalizePlanningSignalStatus(value: unknown): ExternalIedPlanningSignalStatus {
  const status = String(value ?? "not_planned")
  return VALID_PLANNING_SIGNAL_STATUSES.has(status as ExternalIedPlanningSignalStatus)
    ? status as ExternalIedPlanningSignalStatus
    : "not_planned"
}

function normalizeDiscoveryTree(value: unknown): ExternalIedDiscoveryTree | null {
  if (!value || typeof value !== "object" || Array.isArray(value)) {
    return null
  }
  const reportsValue = (value as { reports?: unknown }).reports
  if (!Array.isArray(reportsValue)) {
    return null
  }
  const reports = reportsValue
    .map((rawReport) => {
      if (!rawReport || typeof rawReport !== "object" || Array.isArray(rawReport)) {
        return null
      }
      const report = rawReport as Record<string, unknown>
      const reference = normalizeOptionalText(report.reference)
      if (!reference) {
        return null
      }
      const datasetValue = report.dataset
      const dataset = datasetValue && typeof datasetValue === "object" && !Array.isArray(datasetValue)
        ? (() => {
            const rawDataset = datasetValue as Record<string, unknown>
            const datasetReference = normalizeOptionalText(rawDataset.reference)
            if (!datasetReference) {
              return null
            }
            const rawSignals = Array.isArray(rawDataset.signals) ? rawDataset.signals : []
            return {
              reference: datasetReference,
              signals: rawSignals
                .map((rawSignal) => {
                  if (!rawSignal || typeof rawSignal !== "object" || Array.isArray(rawSignal)) {
                    return null
                  }
                  const signal = rawSignal as Record<string, unknown>
                  const signalReference = normalizeOptionalText(signal.reference)
                  return signalReference
                    ? { reference: signalReference, fc: normalizeOptionalText(signal.fc) }
                    : null
                })
                .filter((signal): signal is { reference: string; fc: string | null } => Boolean(signal)),
            }
          })()
        : null
      return {
        reference,
        name: normalizeOptionalText(report.name) ?? reference,
        kind: normalizeOptionalText(report.kind) ?? "unknown",
        dataset_reference: normalizeOptionalText(report.dataset_reference),
        dataset,
      }
    })
    .filter((report): report is NonNullable<typeof report> => Boolean(report))
  return {
    endpoint: normalizeOptionalText((value as { endpoint?: unknown }).endpoint) ?? "",
    model_fingerprint: normalizeOptionalText((value as { model_fingerprint?: unknown }).model_fingerprint),
    reports,
  }
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
    discoveryState: normalizeDiscoveryState(record.discovery_state),
    discoveryRetryAtMs: normalizeOptionalMs(record.discovery_retry_at_ms),
    discoveryLastError: record.discovery_last_error ?? null,
    discoveryUpdatedAtMs: normalizeOptionalMs(record.discovery_updated_at_ms),
    discoveryReadyForVerification: record.discovery_ready_for_verification === true,
    discoveryDeviceIdentity: normalizeOptionalText(record.discovery_device_identity),
    discoveryVendor: normalizeOptionalText(record.discovery_vendor),
    discoveryModel: normalizeOptionalText(record.discovery_model),
    discoveryDatasets: normalizeOptionalMs(record.discovery_datasets),
    discoveryRcbs: normalizeOptionalMs(record.discovery_rcbs),
    discoveryModelSignals: normalizeOptionalMs(record.discovery_model_signals),
    planningState: normalizePlanningState(record.planning_state),
    planningUpdatedAtMs: normalizeOptionalMs(record.planning_updated_at_ms),
    planningMatchedCount: normalizeOptionalMs(record.planning_matched_count) ?? 0,
    planningUnmatchedCount: normalizeOptionalMs(record.planning_unmatched_count) ?? 0,
    planningAmbiguousCount: normalizeOptionalMs(record.planning_ambiguous_count) ?? 0,
    planningLastError: normalizeOptionalText(record.planning_last_error),
  }
}

function normalizePlanningCoverage(result: ExternalIedPlanningSignalResult): ExternalIedPlanningCoverage | null {
  const signalId = Number(result.signal_id)
  if (!Number.isFinite(signalId) || signalId <= 0) return null
  const endpoint = normalizeOptionalText(result.endpoint)
  if (!endpoint) return null
  return {
    signalId,
    endpoint,
    status: normalizePlanningSignalStatus(result.status),
    address: normalizeOptionalText(result.address),
    reason: normalizeOptionalText(result.reason),
    fcdaReference: normalizeOptionalText(result.fcda_reference),
    datasetReference: normalizeOptionalText(result.dataset_reference),
    rcbReference: normalizeOptionalText(result.rcb_reference),
    rcbName: normalizeOptionalText(result.rcb_name),
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
  const planningCoverageBySignalId = ref<Record<number, ExternalIedPlanningCoverage>>({})
  const discoveryTreeCache = ref<Record<string, ExternalIedDiscoveryTree>>({})
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
          discoveryState: "NeverDiscovered",
          discoveryRetryAtMs: null,
          discoveryLastError: null,
          discoveryUpdatedAtMs: null,
          discoveryReadyForVerification: false,
          discoveryDeviceIdentity: null,
          discoveryVendor: null,
          discoveryModel: null,
          discoveryDatasets: null,
          discoveryRcbs: null,
          discoveryModelSignals: null,
          planningState: "NotPlanned",
          planningUpdatedAtMs: null,
          planningMatchedCount: 0,
          planningUnmatchedCount: 0,
          planningAmbiguousCount: 0,
          planningLastError: null,
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
    const configured = configuredTargets.value[key]
    const effectiveSignalIds = signalIds.length > 0
      ? signalIds
      : previous?.signalIds.length
        ? previous.signalIds
        : configured?.signalIds ?? []
    if (previous?.status === status && previous.lastCheckedAt === event.checked_at) {
      return
    }
    records.value = {
      ...records.value,
      [key]: {
        ip,
        port,
        status,
        signalIds: effectiveSignalIds,
        lastCheckedAt: event.checked_at,
        lastError: event.error ?? null,
        checkKind: event.check_kind === "tcp_connect" ? "tcp_connect" : "none",
        failureCode: event.failure_code ?? null,
        discoveryState: normalizeDiscoveryState(event.discovery_state),
        discoveryRetryAtMs: normalizeOptionalMs(event.discovery_retry_at_ms),
        discoveryLastError: event.discovery_last_error ?? null,
        discoveryUpdatedAtMs: normalizeOptionalMs(event.discovery_updated_at_ms),
        discoveryReadyForVerification: event.discovery_ready_for_verification === true,
        discoveryDeviceIdentity: normalizeOptionalText(event.discovery_device_identity) ?? previous?.discoveryDeviceIdentity ?? null,
        discoveryVendor: normalizeOptionalText(event.discovery_vendor) ?? previous?.discoveryVendor ?? null,
        discoveryModel: normalizeOptionalText(event.discovery_model) ?? previous?.discoveryModel ?? null,
        discoveryDatasets: normalizeOptionalMs(event.discovery_datasets) ?? previous?.discoveryDatasets ?? null,
        discoveryRcbs: normalizeOptionalMs(event.discovery_rcbs) ?? previous?.discoveryRcbs ?? null,
        discoveryModelSignals: normalizeOptionalMs(event.discovery_model_signals) ?? previous?.discoveryModelSignals ?? null,
        planningState: normalizePlanningState(event.planning_state ?? previous?.planningState),
        planningUpdatedAtMs: normalizeOptionalMs(event.planning_updated_at_ms) ?? previous?.planningUpdatedAtMs ?? null,
        planningMatchedCount: normalizeOptionalMs(event.planning_matched_count) ?? previous?.planningMatchedCount ?? 0,
        planningUnmatchedCount: normalizeOptionalMs(event.planning_unmatched_count) ?? previous?.planningUnmatchedCount ?? 0,
        planningAmbiguousCount: normalizeOptionalMs(event.planning_ambiguous_count) ?? previous?.planningAmbiguousCount ?? 0,
        planningLastError: normalizeOptionalText(event.planning_last_error) ?? previous?.planningLastError ?? null,
      },
    }
    publishChanged([ip], effectiveSignalIds)
  }

  function applyPlanningSnapshot(event: ExternalIedPlanningSnapshotEvent) {
    if (Number(event.workspace_id) !== Number(workspaceStore.activeWorkspaceId)) {
      return
    }
    const nextCoverage: Record<number, ExternalIedPlanningCoverage> = {}
    const changedSignalIds = normalizeSignalIds(event.removed_signal_ids ?? [])
    event.signal_results.forEach((result) => {
      const coverage = normalizePlanningCoverage(result)
      if (!coverage) return
      nextCoverage[coverage.signalId] = coverage
      if (JSON.stringify(planningCoverageBySignalId.value[coverage.signalId] ?? null) !== JSON.stringify(coverage)) {
        changedSignalIds.push(coverage.signalId)
      }
    })
    const nextRecords = { ...records.value }
    event.endpoints.forEach((endpoint) => {
      applyPlanningEndpointToRecords(nextRecords, endpoint)
    })
    records.value = nextRecords
    planningCoverageBySignalId.value = nextCoverage
    publishChanged([], changedSignalIds)
  }

  function applyPlanningChanged(event: ExternalIedPlanningChangedEvent) {
    if (Number(event.workspace_id) !== Number(workspaceStore.activeWorkspaceId)) {
      return
    }
    const changedSignalIds = normalizeSignalIds(event.removed_signal_ids ?? [])
    const nextCoverage = { ...planningCoverageBySignalId.value }
    changedSignalIds.forEach(signalId => delete nextCoverage[signalId])
    event.signal_results.forEach((result) => {
      const coverage = normalizePlanningCoverage(result)
      if (!coverage) return
      nextCoverage[coverage.signalId] = coverage
      changedSignalIds.push(coverage.signalId)
    })
    const nextRecords = { ...records.value }
    applyPlanningEndpointToRecords(nextRecords, event.endpoint)
    records.value = nextRecords
    planningCoverageBySignalId.value = nextCoverage
    publishChanged([], changedSignalIds)
  }

  function applyPlanningEndpointToRecords(nextRecords: Record<string, ExternalIedRecord>, endpoint: ExternalIedPlanningEndpointRecord) {
    const ip = normalizeIp(endpoint.ip)
    if (!ip) return
    const key = endpointKey(ip, normalizePort(endpoint.port))
    const previous = nextRecords[key]
    if (!previous) return
    nextRecords[key] = {
      ...previous,
      planningState: normalizePlanningState(endpoint.state),
      planningUpdatedAtMs: normalizeOptionalMs(endpoint.updated_at_ms),
      planningMatchedCount: normalizeOptionalMs(endpoint.matched_count) ?? 0,
      planningUnmatchedCount: normalizeOptionalMs(endpoint.unmatched_count) ?? 0,
      planningAmbiguousCount: normalizeOptionalMs(endpoint.ambiguous_count) ?? 0,
      planningLastError: normalizeOptionalText(endpoint.last_error),
    }
  }

  function clearLocal(signalIds: readonly number[] = Object.values(records.value).flatMap(record => record.signalIds)) {
    records.value = {}
    planningCoverageBySignalId.value = {}
    discoveryTreeCache.value = {}
    configuredTargets.value = {}
    publishChanged([], normalizeSignalIds(signalIds))
  }

  function getStatus(ip: string | null | undefined, port?: number | null): ExternalIedStatus {
    const normalizedIp = normalizeIp(ip)
    if (!normalizedIp) return "not_applicable"
    return records.value[endpointKey(normalizedIp, normalizePort(port))]?.status ?? "not_applicable"
  }

  function getRecord(ip: string | null | undefined, port?: number | null): ExternalIedRecord | null {
    const normalizedIp = normalizeIp(ip)
    if (!normalizedIp) return null
    return records.value[endpointKey(normalizedIp, normalizePort(port))] ?? null
  }

  function getPlanningCoverage(signalId: number | null | undefined): ExternalIedPlanningCoverage | null {
    const normalizedSignalId = Number(signalId)
    if (!Number.isFinite(normalizedSignalId) || normalizedSignalId <= 0) return null
    return planningCoverageBySignalId.value[normalizedSignalId] ?? null
  }

  const active = computed(() => Object.keys(configuredTargets.value).length > 0 || Object.keys(records.value).length > 0)
  const devices = computed(() => Object.values(records.value))
  const summary = computed<ExternalIedSummary>(() => {
    const recordsList = devices.value.filter(record => record.status !== "not_applicable")
    let ready = 0
    let discovering = 0
    let offline = 0
    let failed = 0
    recordsList.forEach((record) => {
      if (record.discoveryState === "Failed" || record.discoveryState === "Cancelled") {
        failed += 1
        return
      }
      if (record.status === "offline") {
        offline += 1
        return
      }
      if (record.discoveryState === "Queued" || record.discoveryState === "Running" || record.discoveryState === "RetryWaiting" || record.discoveryState === "Stale") {
        discovering += 1
        return
      }
      if (record.status === "reachable") {
        ready += 1
      }
    })
    return {
      active: active.value,
      ready,
      discovering,
      offline,
      failed,
    }
  })

  async function refreshDiscovery(ip: string | null | undefined, port?: number | null) {
    const normalizedIp = normalizeIp(ip)
    const workspaceId = Number(workspaceStore.activeWorkspaceId)
    if (!normalizedIp || !Number.isFinite(workspaceId) || workspaceId <= 0) {
      return
    }
    await VerificationAPI.refreshExternalIedDiscovery(workspaceId, normalizedIp, normalizePort(port))
    const key = endpointKey(normalizedIp, normalizePort(port))
    discoveryTreeCache.value = Object.fromEntries(
      Object.entries(discoveryTreeCache.value).filter(([cacheKey]) => cacheKey !== key),
    )
  }

  async function loadDiscoveryTree(ip: string | null | undefined, port?: number | null, force = false) {
    const normalizedIp = normalizeIp(ip)
    const normalizedPort = normalizePort(port)
    const workspaceId = Number(workspaceStore.activeWorkspaceId)
    if (!normalizedIp || !Number.isFinite(workspaceId) || workspaceId <= 0) {
      return null
    }
    const key = endpointKey(normalizedIp, normalizedPort)
    if (!force && discoveryTreeCache.value[key]) {
      return discoveryTreeCache.value[key]
    }
    const response = await VerificationAPI.getExternalIedDiscoveryTree(workspaceId, normalizedIp, normalizedPort)
    const tree = normalizeDiscoveryTree(response.data)
    if (!tree) {
      return null
    }
    const normalizedTree = {
      ...tree,
      endpoint: tree.endpoint || key,
    }
    discoveryTreeCache.value = {
      ...discoveryTreeCache.value,
      [key]: normalizedTree,
    }
    return normalizedTree
  }

  function getDiscoveryTree(ip: string | null | undefined, port?: number | null): ExternalIedDiscoveryTree | null {
    const normalizedIp = normalizeIp(ip)
    if (!normalizedIp) return null
    return discoveryTreeCache.value[endpointKey(normalizedIp, normalizePort(port))] ?? null
  }

  return {
    active,
    records,
    devices,
    summary,
    statusRevision,
    lastChangedIps,
    lastChangedSignalIds,
    lastError,
    configureExpectedDevices,
    applySnapshot,
    applyStatusChanged,
    getStatus,
    getRecord,
    getPlanningCoverage,
    getDiscoveryTree,
    loadDiscoveryTree,
    refreshDiscovery,
    applyPlanningSnapshot,
    applyPlanningChanged,
    clearLocal,
  }
})
