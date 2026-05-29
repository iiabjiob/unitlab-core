import { normalizeReportDataReference } from "./reportEventNormalizer"
import type {
  Iec61850ReportControlCandidate,
  Iec61850ReportControlRef,
  Iec61850ReportEvent,
  Iec61850ReportJsonValue,
  Iec61850ReportSubscriptionPlan,
  Iec61850ReportSubscriptionPlanReport,
  Iec61850ReportSubscriptionPlanSignal,
  Iec61850ReportValue,
} from "./types"

export type Iec61850ReportObservationDiagnosticCode =
  | "REPORT_NOT_IN_PLAN"
  | "SIGNAL_NOT_INCLUDED_IN_REPORT_EVENT"

export type Iec61850ReportObservationDiagnostic = {
  severity: "error" | "warning" | "info"
  code: Iec61850ReportObservationDiagnosticCode
  message: string
  reference: Iec61850ReportControlRef
  signalId?: string
  address?: string
  dataReference?: string | null
}

export type Iec61850SignalObservation = {
  eventId: string
  reportCandidateId: string
  selectedSignalId: string
  selectedSignalAddress: string
  selectedSignalLabel: string | null
  iedName: string
  modelReference: string
  matchKind: Iec61850ReportSubscriptionPlanSignal["matchKind"]
  dataSetIndex: number
  dataReference: string | null
  value: Iec61850ReportJsonValue
  reasonCode: Iec61850ReportValue["reasonCode"]
  timestamp: string
}

export type Iec61850UnselectedReportValue = {
  dataSetIndex: number
  reference: string
  dataReference: string | null
  value: Iec61850ReportJsonValue
}

export type Iec61850ReportObservationResult = {
  eventId: string
  reportCandidateId: string | null
  observations: Iec61850SignalObservation[]
  unselectedValues: Iec61850UnselectedReportValue[]
  diagnostics: Iec61850ReportObservationDiagnostic[]
}

export function mapIec61850ReportEventToSignalObservations(
  plan: Iec61850ReportSubscriptionPlan,
  event: Iec61850ReportEvent,
): Iec61850ReportObservationResult {
  const reportPlan = findReportPlan(plan, event.reportControl)
  if (!reportPlan) {
    return {
      eventId: event.id,
      reportCandidateId: null,
      observations: [],
      unselectedValues: event.values.map(toUnselectedReportValue),
      diagnostics: [{
        severity: "error",
        code: "REPORT_NOT_IN_PLAN",
        message: "Report event does not match any required ReportControl in the subscription plan.",
        reference: event.reportControl,
      }],
    }
  }

  return mapIec61850ReportPlanEventToSignalObservations(reportPlan, event)
}

export function mapIec61850ReportPlanEventToSignalObservations(
  reportPlan: Iec61850ReportSubscriptionPlanReport,
  event: Iec61850ReportEvent,
): Iec61850ReportObservationResult {
  const diagnostics: Iec61850ReportObservationDiagnostic[] = []
  const candidate = reportPlan.candidate
  const valuesByReference = buildValueIndex(candidate, event.values)
  const selectedReferences = new Set<string>()
  const observations: Iec61850SignalObservation[] = []

  for (const signal of reportPlan.matchedSignals) {
    const key = normalizeObservationReference(signal.modelReference, candidate)
    selectedReferences.add(key)
    const value = valuesByReference.get(key)
    if (!value) {
      diagnostics.push({
        severity: "info",
        code: "SIGNAL_NOT_INCLUDED_IN_REPORT_EVENT",
        message: `Selected signal "${signal.selectedSignal.address}" was not included in this report event.`,
        reference: event.reportControl,
        signalId: signal.selectedSignal.id,
        address: signal.selectedSignal.address,
        dataReference: signal.modelReference,
      })
      continue
    }

    observations.push({
      eventId: event.id,
      reportCandidateId: candidate.id,
      selectedSignalId: signal.selectedSignal.id,
      selectedSignalAddress: signal.selectedSignal.address,
      selectedSignalLabel: signal.selectedSignal.label ?? null,
      iedName: signal.iedName,
      modelReference: signal.modelReference,
      matchKind: signal.matchKind,
      dataSetIndex: value.dataSetIndex,
      dataReference: value.dataReference,
      value: value.value,
      reasonCode: value.reasonCode,
      timestamp: value.timestamp,
    })
  }

  const unselectedValues = event.values
    .filter(value => !selectedReferences.has(normalizeObservationReference(value.reference, candidate)))
    .map(toUnselectedReportValue)

  return {
    eventId: event.id,
    reportCandidateId: candidate.id,
    observations: observations.sort(compareObservations),
    unselectedValues,
    diagnostics,
  }
}

function findReportPlan(
  plan: Iec61850ReportSubscriptionPlan,
  reference: Iec61850ReportControlRef,
): Iec61850ReportSubscriptionPlanReport | null {
  for (const device of plan.devices) {
    for (const report of device.reports) {
      if (sameReportControl(report.candidate, reference)) {
        return report
      }
    }
  }
  return null
}

function sameReportControl(
  candidate: Iec61850ReportControlCandidate,
  reference: Iec61850ReportControlRef,
): boolean {
  return candidate.iedName === reference.iedName
    && candidate.accessPointName === reference.accessPointName
    && candidate.logicalDeviceInst === reference.logicalDeviceInst
    && candidate.logicalNodeName === reference.logicalNodeName
    && candidate.reportControlName === reference.reportControlName
    && candidate.reportKind === reference.reportKind
}

function buildValueIndex(
  candidate: Iec61850ReportControlCandidate,
  values: readonly Iec61850ReportValue[],
): Map<string, Iec61850ReportValue> {
  const index = new Map<string, Iec61850ReportValue>()
  for (const value of values) {
    index.set(normalizeObservationReference(value.reference, candidate), value)
  }
  return index
}

function normalizeObservationReference(
  reference: string,
  candidate: Iec61850ReportControlCandidate,
): string {
  return normalizeReportDataReference(reference, candidate).toLowerCase()
}

function toUnselectedReportValue(value: Iec61850ReportValue): Iec61850UnselectedReportValue {
  return {
    dataSetIndex: value.dataSetIndex,
    reference: value.reference,
    dataReference: value.dataReference,
    value: value.value,
  }
}

function compareObservations(left: Iec61850SignalObservation, right: Iec61850SignalObservation): number {
  return `${left.dataSetIndex}/${left.selectedSignalId}`.localeCompare(`${right.dataSetIndex}/${right.selectedSignalId}`)
}
