import { mergeIec61850SignalList } from "@/pages/debug61850/iec61850SignalListMerge"
import type { Iec61850DebugDocument } from "@/pages/debug61850/iec61850DebugTree"
import type { SignalAllocationRow } from "@/types/signal"
import type { Online61850PreparationTarget } from "./online61850Targets"

export type Online61850TargetSignalMatchResult = {
  matchedSignalIds: Set<number>
  matchedSignalReferencesById: Map<number, string>
  matchedReports: Array<{
    name: string
    kind: string
    dataSetRef: string | null
    iedName: string | null
  }>
  matchedSignalCount: number
  unmatchedSignalCount: number
}

export function matchOnline61850TargetSignals(
  rows: readonly SignalAllocationRow[],
  discovery: unknown,
  target: Pick<Online61850PreparationTarget, "iedName" | "accessPointName">,
): Online61850TargetSignalMatchResult {
  const document = buildDiscoveryDocumentFromTargetDiscovery(discovery, target)
  const mergeResult = mergeIec61850SignalList(document, rows, null)
  const matchedSignalIds = new Set<number>(
    mergeResult.matches
      .map(match => Number(match.signalId))
      .filter(signalId => Number.isFinite(signalId)),
  )
  const matchedSignalReferencesById = new Map<number, string>()
  mergeResult.matches.forEach((match) => {
    const signalId = Number(match.signalId)
    const modelReference = String(match.modelReference ?? "").trim()
    if (!Number.isFinite(signalId) || !modelReference || matchedSignalReferencesById.has(signalId)) {
      return
    }
    matchedSignalReferencesById.set(signalId, modelReference)
  })

  return {
    matchedSignalIds,
    matchedSignalReferencesById,
    matchedReports: mergeResult.matchedReports,
    matchedSignalCount: mergeResult.matchedRows,
    unmatchedSignalCount: mergeResult.unmatchedRows + mergeResult.rowsWithoutAddress,
  }
}

function buildDiscoveryDocumentFromTargetDiscovery(
  discovery: unknown,
  target: Pick<Online61850PreparationTarget, "iedName" | "accessPointName">,
): Iec61850DebugDocument {
  const payload = isRecord(discovery) ? discovery : {}
  const endpoint = isRecord(payload.endpoint) ? payload.endpoint : {}
  const iedName = trimToNull(stringValue(endpoint.iedName) ?? stringValue(endpoint.ied_name) ?? target.iedName) ?? ""
  const accessPointName = trimToNull(stringValue(endpoint.accessPointName) ?? stringValue(endpoint.access_point_name) ?? target.accessPointName) ?? "AP1"
  const refs = collectDiscoveryReferences(payload)
  const dataSets = collectDiscoveryDataSets(payload)
  const discoveredReportSignals = collectDiscoveryReportSignals(payload, iedName, accessPointName, dataSets)
  const reportSignals = discoveredReportSignals.length > 0
    ? discoveredReportSignals
    : refs.map(reference => ({
      reference,
      reportControlId: "discovery",
      reportControlName: "discovery",
      reportKind: "BRCB" as const,
      dataSetId: "discovery",
      dataSetRef: "discovery",
      iedName,
      accessPointName,
    }))

  const signalInventory = refs.map(reference => ({
    reference,
    dataSetId: "discovery",
    dataSetRef: "discovery",
    iedName,
    accessPointName,
  }))

  return {
    stats: {
      sites: 0,
      voltageLevels: 0,
      bays: 0,
      switchgears: 0,
      ieds: iedName ? 1 : 0,
      logicalDevices: 0,
      dataSets: signalInventory.length > 0 ? 1 : 0,
      reports: 0,
      reportSignals: signalInventory.length,
    },
    diagnosticSummary: {
      error: 0,
      warning: 0,
      info: 0,
      total: 0,
      rendered: 0,
      omitted: 0,
    },
    diagnostics: [],
    reportCandidates: [],
    signalInventory: {
      dataSetSignals: signalInventory,
      reportSignals,
    },
    treeRows: [],
  }
}

function collectDiscoveryDataSets(rawDiscovery: Record<string, unknown>): Array<{ reference: string; members: string[] }> {
  const dataSets: Array<{ reference: string; members: string[] }> = []
  const items = Array.isArray(rawDiscovery.dataSets) ? rawDiscovery.dataSets : []
  items.forEach((item) => {
    if (!isRecord(item)) return
    const reference = normalizeReference(stringValue(item.reference))
    if (!reference) return
    const members = Array.isArray(item.members) ? item.members : []
    const memberReferences = new Set<string>()
    members.forEach((member) => {
      if (!isRecord(member)) return
      collectReferenceVariants(member).forEach(referenceValue => memberReferences.add(referenceValue))
    })
    dataSets.push({
      reference,
      members: [...memberReferences],
    })
  })
  return dataSets
}

function collectDiscoveryReportSignals(
  rawDiscovery: Record<string, unknown>,
  iedName: string,
  accessPointName: string,
  dataSets: Array<{ reference: string; members: string[] }>,
): Iec61850DebugDocument["signalInventory"]["reportSignals"] {
  const reportSignals: Iec61850DebugDocument["signalInventory"]["reportSignals"] = []
  const seen = new Set<string>()
  const reportControls = Array.isArray(rawDiscovery.reportControls) ? rawDiscovery.reportControls : []
  reportControls.forEach((reportControl) => {
    if (!isRecord(reportControl)) return
    const reportControlName = stringValue(reportControl.name) || "discovery"
    const reportKind = (stringValue(reportControl.kind) || "BRCB") as "BRCB" | "URCB"
    const dataSetRef = stringValue(reportControl.dataSetRef) || null
    const dataSet = dataSetRef
      ? dataSets.find(item => item.reference === normalizeReference(dataSetRef))
      : null
    const references = dataSet?.members ?? []
    references.forEach((reference) => {
      const normalizedReference = normalizeReference(reference)
      if (!normalizedReference) return
      const key = `${normalizedReference}|${reportControlName}|${reportKind}|${dataSetRef ?? ""}`
      if (seen.has(key)) return
      seen.add(key)
      reportSignals.push({
        reference: normalizedReference,
        reportControlId: stringValue(reportControl.id) || reportControlName,
        reportControlName,
        reportKind,
        dataSetId: dataSetRef ?? "discovery",
        dataSetRef,
        iedName,
        accessPointName,
      })
    })
  })
  return reportSignals
}

function collectDiscoveryReferences(rawDiscovery: Record<string, unknown>): string[] {
  const references = new Set<string>()

  const signals = Array.isArray(rawDiscovery.signals) ? rawDiscovery.signals : []
  signals.forEach((item) => {
    if (!isRecord(item)) return
    collectReferenceVariants(item).forEach(reference => references.add(reference))
  })

  const dataSets = Array.isArray(rawDiscovery.dataSets) ? rawDiscovery.dataSets : []
  dataSets.forEach((dataSet) => {
    if (!isRecord(dataSet)) return
    const members = Array.isArray(dataSet.members) ? dataSet.members : []
    members.forEach((member) => {
      if (!isRecord(member)) return
      collectReferenceVariants(member).forEach(reference => references.add(reference))
    })
  })

  return [...references]
}

function collectReferenceVariants(item: Record<string, unknown>): string[] {
  return [
    stringValue(item.reference),
    stringValue(item.mmsReference),
    stringValue(item.displayReference),
    stringValue(item.dataRef),
  ]
    .map(reference => normalizeReference(reference))
    .filter(Boolean)
}

function normalizeReference(value: string | null | undefined): string {
  return String(value ?? "")
    .trim()
    .replace(/\\/g, "/")
    .replace(/\s+/g, "")
    .toLowerCase()
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return Boolean(value) && typeof value === "object" && !Array.isArray(value)
}

function stringValue(value: unknown): string {
  return typeof value === "string" ? value : ""
}

function trimToNull(value: string): string | null {
  const text = value.trim()
  return text ? text : null
}
