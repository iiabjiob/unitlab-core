import {
  buildIec61850ReportSubscriptionPlan,
  createIec61850SimulatorAdapter,
  Iec61850ReportManager,
  type Iec61850DeviceEndpoint,
  type Iec61850ReportControlCandidate,
  type Iec61850ReportControlReadResult,
  type Iec61850ReportEvent,
  type Iec61850ReportRuntimeDiagnostic,
  type Iec61850ReportSubscriptionPlan,
  type Iec61850ReportSubscriptionPlanDiagnostic,
  type Iec61850SelectedSignal,
  type Iec61850SimulatorEvent,
} from "@/modules/iec61850-report-core"
import type { Iec61850DebugDocument } from "./iec61850DebugTree"
import type { Iec61850SignalListMergeResult } from "./iec61850SignalListMerge"

export type Iec61850DebugSimulatorReportResult = {
  candidateId: string
  iedName: string
  accessPointName: string
  reportControlName: string
  reportKind: "buffered" | "unbuffered"
  dataSetRef: string | null
  signalCount: number
  matchedSignalCount: number
  lifecycleState: string
  diagnostics: Iec61850ReportRuntimeDiagnostic[]
  event: Iec61850ReportEvent | null
  errorCode: string | null
  errorMessage: string | null
}

export type Iec61850DebugSimulatorRunResult = {
  plan: Iec61850ReportSubscriptionPlan
  reports: Iec61850DebugSimulatorReportResult[]
  diagnostics: Array<Iec61850ReportSubscriptionPlanDiagnostic | Iec61850ReportRuntimeDiagnostic>
  eventLog: Iec61850SimulatorEvent[]
  startedAt: string
  finishedAt: string
}

const DEBUG_SIMULATOR_CLIENT_ID = "unitlab-debug-simulator"

export function buildIec61850DebugSimulatorPlan(
  document: Iec61850DebugDocument,
  mergeResult: Iec61850SignalListMergeResult,
): Iec61850ReportSubscriptionPlan {
  return buildIec61850ReportSubscriptionPlan({
    candidates: document.reportCandidates,
    selectedSignals: mergeResult.matches.map((match): Iec61850SelectedSignal => ({
      id: String(match.signalId),
      address: match.address,
      label: match.signalName || match.signalKey,
    })),
  })
}

export async function runIec61850DebugSimulator(
  document: Iec61850DebugDocument,
  mergeResult: Iec61850SignalListMergeResult,
  now: () => Date = () => new Date(),
): Promise<Iec61850DebugSimulatorRunResult> {
  const startedAt = now().toISOString()
  const plan = buildIec61850DebugSimulatorPlan(document, mergeResult)
  const requiredReports = plan.devices.flatMap(device => device.reports)
  const devices = buildSimulatorDevices(requiredReports.map(report => report.candidate))
  const adapter = createIec61850SimulatorAdapter({ devices, now })
  const manager = new Iec61850ReportManager(adapter)
  const reports: Iec61850DebugSimulatorReportResult[] = []
  const diagnostics: Iec61850DebugSimulatorRunResult["diagnostics"] = [...plan.diagnostics]

  for (const reportPlan of requiredReports) {
    const candidate = reportPlan.candidate
    const endpoint = endpointFromCandidate(candidate)
    let readResult: Iec61850ReportControlReadResult | null = null
    try {
      readResult = await manager.readReportControl(endpoint, candidate)
      diagnostics.push(...readResult.diagnostics)
      await manager.reserveReportControl(endpoint, candidate, DEBUG_SIMULATOR_CLIENT_ID)
      await manager.enableReportControl(endpoint, candidate, DEBUG_SIMULATOR_CLIENT_ID)
      const event = await manager.sendGeneralInterrogation(endpoint, candidate, DEBUG_SIMULATOR_CLIENT_ID)
      const disabledState = await manager.disableReportControl(endpoint, candidate, DEBUG_SIMULATOR_CLIENT_ID)
      await manager.releaseReportControl(endpoint, candidate, DEBUG_SIMULATOR_CLIENT_ID)

      reports.push({
        candidateId: candidate.id,
        iedName: candidate.iedName,
        accessPointName: candidate.accessPointName,
        reportControlName: candidate.reportControlName,
        reportKind: candidate.reportKind,
        dataSetRef: candidate.dataSetRef,
        signalCount: candidate.signalCount,
        matchedSignalCount: reportPlan.matchedSignals.length,
        lifecycleState: disabledState.lifecycleState,
        diagnostics: readResult.diagnostics,
        event,
        errorCode: null,
        errorMessage: null,
      })
    } catch (error) {
      const errorCode = error instanceof Error && "code" in error
        ? String((error as { code?: unknown }).code)
        : "SIMULATOR_ERROR"
      reports.push({
        candidateId: candidate.id,
        iedName: candidate.iedName,
        accessPointName: candidate.accessPointName,
        reportControlName: candidate.reportControlName,
        reportKind: candidate.reportKind,
        dataSetRef: candidate.dataSetRef,
        signalCount: candidate.signalCount,
        matchedSignalCount: reportPlan.matchedSignals.length,
        lifecycleState: readResult?.state.lifecycleState ?? "failed",
        diagnostics: readResult?.diagnostics ?? [],
        event: null,
        errorCode,
        errorMessage: error instanceof Error ? error.message : "IEC 61850 simulator failed.",
      })
    }
  }

  return {
    plan,
    reports,
    diagnostics,
    eventLog: adapter.getEventLog(),
    startedAt,
    finishedAt: now().toISOString(),
  }
}

function buildSimulatorDevices(candidates: Iec61850ReportControlCandidate[]) {
  const reportsByEndpoint = new Map<string, {
    endpoint: Iec61850DeviceEndpoint
    reports: Iec61850ReportControlCandidate[]
  }>()

  for (const candidate of candidates) {
    const endpoint = endpointFromCandidate(candidate)
    const existing = reportsByEndpoint.get(endpoint.id)
    if (existing) {
      existing.reports.push(candidate)
      continue
    }
    reportsByEndpoint.set(endpoint.id, {
      endpoint,
      reports: [candidate],
    })
  }

  return Array.from(reportsByEndpoint.values())
}

function endpointFromCandidate(candidate: Iec61850ReportControlCandidate): Iec61850DeviceEndpoint {
  return {
    id: `sim:${candidate.iedName}/${candidate.accessPointName}`,
    mode: "simulator",
    iedName: candidate.iedName,
    accessPointName: candidate.accessPointName,
    host: null,
    port: 102,
  }
}
