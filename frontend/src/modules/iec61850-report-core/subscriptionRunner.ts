import { Iec61850ReportManager, toReportControlRef } from "./reportManager"
import {
  createIec61850SimulatorAdapter,
  type Iec61850SimulatorDevice,
  type Iec61850SimulatorEvent,
} from "./simulator"
import type {
  Iec61850DeviceEndpoint,
  Iec61850ReportControlCandidate,
  Iec61850ReportControlReadResult,
  Iec61850ReportControlState,
  Iec61850ReportEvent,
  Iec61850ReportLifecycleState,
  Iec61850ReportManagerAdapter,
  Iec61850ReportRuntimeDiagnostic,
  Iec61850ReportSubscriptionPlan,
  Iec61850ReportSubscriptionPlanDiagnostic,
  Iec61850ReportSubscriptionPlanReport,
} from "./types"

export type Iec61850ReportSubscriptionRunReportResult = {
  candidateId: string
  iedName: string
  accessPointName: string
  reportControlName: string
  reportKind: "buffered" | "unbuffered"
  dataSetRef: string | null
  signalCount: number
  matchedSignalCount: number
  lifecycleState: Iec61850ReportLifecycleState
  diagnostics: Iec61850ReportRuntimeDiagnostic[]
  event: Iec61850ReportEvent | null
  errorCode: string | null
  errorMessage: string | null
}

export type Iec61850ReportSubscriptionRunResult = {
  plan: Iec61850ReportSubscriptionPlan
  reports: Iec61850ReportSubscriptionRunReportResult[]
  diagnostics: Array<Iec61850ReportSubscriptionPlanDiagnostic | Iec61850ReportRuntimeDiagnostic>
  startedAt: string
  finishedAt: string
}

export type Iec61850SimulatorSubscriptionRunResult = Iec61850ReportSubscriptionRunResult & {
  eventLog: Iec61850SimulatorEvent[]
}

export type RunIec61850ReportSubscriptionPlanOptions = {
  plan: Iec61850ReportSubscriptionPlan
  adapter: Iec61850ReportManagerAdapter
  clientId: string
  endpointForCandidate: (candidate: Iec61850ReportControlCandidate) => Iec61850DeviceEndpoint
  now?: () => Date
}

export type RunIec61850SimulatorSubscriptionPlanOptions = {
  plan: Iec61850ReportSubscriptionPlan
  clientId?: string
  now?: () => Date
}

const DEFAULT_SIMULATOR_CLIENT_ID = "unitlab-iec61850-simulator"

export async function runIec61850ReportSubscriptionPlan(
  options: RunIec61850ReportSubscriptionPlanOptions,
): Promise<Iec61850ReportSubscriptionRunResult> {
  const now = options.now ?? (() => new Date())
  const manager = new Iec61850ReportManager(options.adapter)
  const startedAt = now().toISOString()
  const reports: Iec61850ReportSubscriptionRunReportResult[] = []
  const diagnostics: Iec61850ReportSubscriptionRunResult["diagnostics"] = [...options.plan.diagnostics]

  for (const reportPlan of requiredReports(options.plan)) {
    const report = await runReportPlan({
      reportPlan,
      endpoint: options.endpointForCandidate(reportPlan.candidate),
      manager,
      clientId: options.clientId,
    })
    reports.push(report)
    diagnostics.push(...report.diagnostics)
  }

  return {
    plan: options.plan,
    reports,
    diagnostics,
    startedAt,
    finishedAt: now().toISOString(),
  }
}

export async function runIec61850SimulatorSubscriptionPlan(
  options: RunIec61850SimulatorSubscriptionPlanOptions,
): Promise<Iec61850SimulatorSubscriptionRunResult> {
  const adapter = createIec61850SimulatorAdapter({
    devices: buildIec61850SimulatorDevicesFromPlan(options.plan),
    now: options.now,
  })
  const result = await runIec61850ReportSubscriptionPlan({
    plan: options.plan,
    adapter,
    clientId: options.clientId ?? DEFAULT_SIMULATOR_CLIENT_ID,
    endpointForCandidate: buildIec61850SimulatorEndpoint,
    now: options.now,
  })

  return {
    ...result,
    eventLog: adapter.getEventLog(),
  }
}

export function buildIec61850SimulatorDevicesFromPlan(
  plan: Iec61850ReportSubscriptionPlan,
): Iec61850SimulatorDevice[] {
  const reportsByEndpoint = new Map<string, {
    endpoint: Iec61850DeviceEndpoint
    reports: Iec61850ReportControlCandidate[]
  }>()

  for (const reportPlan of requiredReports(plan)) {
    const endpoint = buildIec61850SimulatorEndpoint(reportPlan.candidate)
    const existing = reportsByEndpoint.get(endpoint.id)
    if (existing) {
      existing.reports.push(reportPlan.candidate)
      continue
    }
    reportsByEndpoint.set(endpoint.id, {
      endpoint,
      reports: [reportPlan.candidate],
    })
  }

  return Array.from(reportsByEndpoint.values())
}

export function buildIec61850SimulatorEndpoint(
  candidate: Iec61850ReportControlCandidate,
): Iec61850DeviceEndpoint {
  return {
    id: `sim:${candidate.iedName}/${candidate.accessPointName}`,
    mode: "simulator",
    iedName: candidate.iedName,
    accessPointName: candidate.accessPointName,
    host: null,
    port: 102,
  }
}

function requiredReports(plan: Iec61850ReportSubscriptionPlan): Iec61850ReportSubscriptionPlanReport[] {
  return plan.devices.flatMap(device => device.reports)
}

async function runReportPlan(input: {
  reportPlan: Iec61850ReportSubscriptionPlanReport
  endpoint: Iec61850DeviceEndpoint
  manager: Iec61850ReportManager
  clientId: string
}): Promise<Iec61850ReportSubscriptionRunReportResult> {
  const { reportPlan, endpoint, manager, clientId } = input
  const candidate = reportPlan.candidate
  const diagnostics: Iec61850ReportRuntimeDiagnostic[] = []
  let readResult: Iec61850ReportControlReadResult | null = null
  let lastState: Iec61850ReportControlState | null = null
  let event: Iec61850ReportEvent | null = null
  let reserved = false
  let enabled = false
  let errorCode: string | null = null
  let errorMessage: string | null = null

  try {
    readResult = await manager.readReportControl(endpoint, candidate)
    lastState = readResult.state
    diagnostics.push(...readResult.diagnostics)
    lastState = await manager.reserveReportControl(endpoint, candidate, clientId)
    reserved = true
    lastState = await manager.enableReportControl(endpoint, candidate, clientId)
    enabled = true
    event = await manager.sendGeneralInterrogation(endpoint, candidate, clientId)
  } catch (error) {
    const normalized = normalizeRunError(error)
    errorCode = normalized.code
    errorMessage = normalized.message
  } finally {
    const cleanup = await cleanupReportControl({
      manager,
      endpoint,
      candidate,
      clientId,
      enabled,
      reserved,
    })
    if (cleanup.lastState) {
      lastState = cleanup.lastState
    }
    diagnostics.push(...cleanup.diagnostics)
    if (!errorCode && cleanup.diagnostics.length) {
      errorCode = cleanup.diagnostics[0]?.code ?? "REPORT_CLEANUP_FAILED"
      errorMessage = cleanup.diagnostics[0]?.message ?? "IEC 61850 report cleanup failed."
    }
  }

  return {
    candidateId: candidate.id,
    iedName: candidate.iedName,
    accessPointName: candidate.accessPointName,
    reportControlName: candidate.reportControlName,
    reportKind: candidate.reportKind,
    dataSetRef: candidate.dataSetRef,
    signalCount: candidate.signalCount,
    matchedSignalCount: reportPlan.matchedSignals.length,
    lifecycleState: lastState?.lifecycleState ?? (errorCode ? "failed" : "disconnected"),
    diagnostics,
    event,
    errorCode,
    errorMessage,
  }
}

async function cleanupReportControl(input: {
  manager: Iec61850ReportManager
  endpoint: Iec61850DeviceEndpoint
  candidate: Iec61850ReportControlCandidate
  clientId: string
  enabled: boolean
  reserved: boolean
}): Promise<{
  lastState: Iec61850ReportControlState | null
  diagnostics: Iec61850ReportRuntimeDiagnostic[]
}> {
  const diagnostics: Iec61850ReportRuntimeDiagnostic[] = []
  let lastState: Iec61850ReportControlState | null = null

  if (input.enabled) {
    try {
      lastState = await input.manager.disableReportControl(input.endpoint, input.candidate, input.clientId)
    } catch (error) {
      diagnostics.push(runtimeDiagnosticFromError(input.candidate, "DISABLE_CLEANUP_FAILED", error))
    }
  }

  if (input.reserved) {
    try {
      lastState = await input.manager.releaseReportControl(input.endpoint, input.candidate, input.clientId)
    } catch (error) {
      diagnostics.push(runtimeDiagnosticFromError(input.candidate, "RELEASE_CLEANUP_FAILED", error))
    }
  }

  return { lastState, diagnostics }
}

function runtimeDiagnosticFromError(
  candidate: Iec61850ReportControlCandidate,
  code: string,
  error: unknown,
): Iec61850ReportRuntimeDiagnostic {
  return {
    severity: "error",
    code,
    message: error instanceof Error ? error.message : "IEC 61850 report cleanup failed.",
    reference: toReportControlRef(candidate),
  }
}

function normalizeRunError(error: unknown): { code: string; message: string } {
  const code = error instanceof Error && "code" in error
    ? String((error as { code?: unknown }).code)
    : "REPORT_RUN_FAILED"
  return {
    code,
    message: error instanceof Error ? error.message : "IEC 61850 report run failed.",
  }
}
