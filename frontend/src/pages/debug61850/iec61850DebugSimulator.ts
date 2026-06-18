import {
  buildIec61850ReportSubscriptionPlan,
  runIec61850SimulatorSubscriptionPlan,
  type Iec61850ReportSubscriptionRunReportResult,
  type Iec61850ReportSubscriptionPlan,
  type Iec61850SelectedSignal,
  type Iec61850SimulatorSubscriptionRunResult,
} from "@/modules/iec61850-report-core"
import type { Iec61850DebugDocument } from "./iec61850DebugTree"
import type { Iec61850SignalListMergeResult } from "./iec61850SignalListMerge"

export type Iec61850DebugSimulatorReportResult = Iec61850ReportSubscriptionRunReportResult
export type Iec61850DebugSimulatorRunResult = Iec61850SimulatorSubscriptionRunResult

const DEBUG_SIMULATOR_CLIENT_ID = "unitlab-debug-simulator"

export function buildIec61850DebugSimulatorPlan(
  document: Iec61850DebugDocument,
  mergeResult: Iec61850SignalListMergeResult,
): Iec61850ReportSubscriptionPlan {
  return buildIec61850ReportSubscriptionPlan({
    candidates: document.reportCandidates,
    selectedSignals: mergeResult.matches.map((match): Iec61850SelectedSignal => ({
      id: String(match.signalId),
      address: match.modelReference || match.address,
      label: match.signalName || match.signalKey,
    })),
  })
}

export async function runIec61850DebugSimulator(
  document: Iec61850DebugDocument,
  mergeResult: Iec61850SignalListMergeResult,
  now: () => Date = () => new Date(),
): Promise<Iec61850DebugSimulatorRunResult> {
  const plan = buildIec61850DebugSimulatorPlan(document, mergeResult)
  return runIec61850SimulatorSubscriptionPlan({
    plan,
    clientId: DEBUG_SIMULATOR_CLIENT_ID,
    now,
  })
}
