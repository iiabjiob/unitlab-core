import { toReportControlRef } from "./reportManager"
import type {
  Iec61850ReportControlCandidate,
  Iec61850ReportControlRef,
  Iec61850ReportEvent,
  Iec61850ReportJsonValue,
  Iec61850ReportReason,
  Iec61850ReportValue,
} from "./types"
import { getIec61850ReportCandidateSignals } from "./normalizedSignals"

export type Iec61850ReportPayloadValue = {
  dataReference?: string | null
  value: Iec61850ReportJsonValue
  reasonCode?: Iec61850ReportReason | null
  timestamp?: string | null
}

export type Iec61850ReportPayload = {
  rptId?: string | null
  dataSetRef?: string | null
  confRev?: string | null
  sequenceNumber?: number | null
  timeOfEntry?: string | null
  entryId?: string | null
  bufferOverflow?: boolean | null
  reason?: Iec61850ReportReason | null
  values: Iec61850ReportPayloadValue[]
}

export type Iec61850ReportEventDiagnosticCode =
  | "MISSING_SEQUENCE_NUMBER"
  | "DUPLICATE_SEQUENCE_NUMBER"
  | "OUT_OF_ORDER_SEQUENCE_NUMBER"
  | "MISSING_TIME_OF_ENTRY"
  | "MISSING_REASON_CODE"
  | "MISSING_DATASET_REF"
  | "MISSING_DATA_REFERENCE"
  | "MISSING_ENTRY_ID"
  | "MISSING_CONFREV"
  | "MISSING_BUFFER_OVERFLOW"
  | "CONFREV_MISMATCH"
  | "DATASET_MISMATCH"
  | "VALUE_COUNT_MISMATCH"
  | "UNKNOWN_DATA_REFERENCE"
  | "DUPLICATE_DATA_REFERENCE"
  | "DATA_REFERENCE_AMBIGUOUS"

export type Iec61850ReportEventDiagnostic = {
  severity: "error" | "warning" | "info"
  code: Iec61850ReportEventDiagnosticCode
  message: string
  reference: Iec61850ReportControlRef
  valueIndex?: number
  dataReference?: string | null
}

export type NormalizeIec61850ReportEventInput = {
  endpointId: string
  candidate: Iec61850ReportControlCandidate
  payload: Iec61850ReportPayload
  receivedAt: string
  reportControl?: Iec61850ReportControlRef
  previousSequenceNumber?: number | null
}

export type NormalizeIec61850ReportEventResult = {
  event: Iec61850ReportEvent
  diagnostics: Iec61850ReportEventDiagnostic[]
}

type AssignedValue = {
  index: number
  payloadValue: Iec61850ReportPayloadValue
}

type ReferenceIndex = Map<string, number[]>

export function normalizeIec61850ReportEvent(
  input: NormalizeIec61850ReportEventInput,
): NormalizeIec61850ReportEventResult {
  const reportControl = input.reportControl ?? toReportControlRef(input.candidate)
  const diagnostics: Iec61850ReportEventDiagnostic[] = []
  const payload = input.payload
  const reason = resolveReportReason(payload)
  const sequenceNumber = payload.sequenceNumber ?? null
  const dataSetRef = payload.dataSetRef ?? null
  const confRev = payload.confRev ?? null
  const values = mapPayloadValues(input, reportControl, reason, diagnostics)

  collectHeaderDiagnostics(input, reportControl, diagnostics)

  const event: Iec61850ReportEvent = {
    id: buildReportEventId(input.endpointId, reportControl, sequenceNumber, input.receivedAt),
    endpointId: input.endpointId,
    receivedAt: input.receivedAt,
    reportControl,
    rptId: payload.rptId ?? input.candidate.rptId,
    dataSetRef,
    confRev,
    sequenceNumber,
    timeOfEntry: payload.timeOfEntry ?? null,
    entryId: payload.entryId ?? null,
    bufferOverflow: payload.bufferOverflow ?? null,
    reason,
    values,
  }

  return { event, diagnostics }
}

function collectHeaderDiagnostics(
  input: NormalizeIec61850ReportEventInput,
  reference: Iec61850ReportControlRef,
  diagnostics: Iec61850ReportEventDiagnostic[],
) {
  const { candidate, payload, previousSequenceNumber } = input
  const optionalFields = candidate.optionalFields
  const hasValueReasonCode = payload.values.some(value => value.reasonCode != null)
  const hasValueTimestamp = payload.values.some(value => value.timestamp != null)
  const hasValueDataReference = payload.values.some(value => value.dataReference != null && value.dataReference.trim() !== "")

  if (optionalFields.sequenceNumber === true && payload.sequenceNumber == null) {
    pushDiagnostic(diagnostics, "warning", "MISSING_SEQUENCE_NUMBER", "Report payload is missing SeqNum although SCD OptFields.seqNum is enabled.", reference)
  }
  if (payload.sequenceNumber != null && previousSequenceNumber != null) {
    if (payload.sequenceNumber === previousSequenceNumber) {
      pushDiagnostic(diagnostics, "warning", "DUPLICATE_SEQUENCE_NUMBER", `Report sequence number ${payload.sequenceNumber} duplicates the previous report.`, reference)
    } else if (payload.sequenceNumber < previousSequenceNumber) {
      pushDiagnostic(diagnostics, "warning", "OUT_OF_ORDER_SEQUENCE_NUMBER", `Report sequence number ${payload.sequenceNumber} is older than previous ${previousSequenceNumber}.`, reference)
    }
  }
  if (optionalFields.timestamp === true && payload.timeOfEntry == null && !hasValueTimestamp) {
    pushDiagnostic(diagnostics, "warning", "MISSING_TIME_OF_ENTRY", "Report payload is missing TimeOfEntry although SCD OptFields.timeStamp is enabled.", reference)
  }
  if (optionalFields.reasonCode === true && payload.reason == null && !hasValueReasonCode) {
    pushDiagnostic(diagnostics, "warning", "MISSING_REASON_CODE", "Report payload is missing reason code although SCD OptFields.reasonCode is enabled.", reference)
  }
  if (optionalFields.dataSetName === true && payload.dataSetRef == null) {
    pushDiagnostic(diagnostics, "warning", "MISSING_DATASET_REF", "Report payload is missing DataSet reference although SCD OptFields.dataSet is enabled.", reference)
  }
  if (optionalFields.dataReference === true && !hasValueDataReference) {
    pushDiagnostic(diagnostics, "warning", "MISSING_DATA_REFERENCE", "Report payload has no data references although SCD OptFields.dataRef is enabled.", reference)
  }
  if (optionalFields.entryId === true && payload.entryId == null) {
    pushDiagnostic(diagnostics, "warning", "MISSING_ENTRY_ID", "Report payload is missing EntryID although SCD OptFields.entryID is enabled.", reference)
  }
  if (optionalFields.configRevision === true && payload.confRev == null) {
    pushDiagnostic(diagnostics, "warning", "MISSING_CONFREV", "Report payload is missing ConfRev although SCD OptFields.configRef is enabled.", reference)
  }
  if (optionalFields.bufferOverflow === true && payload.bufferOverflow == null) {
    pushDiagnostic(diagnostics, "warning", "MISSING_BUFFER_OVERFLOW", "Report payload is missing BufOvfl although SCD OptFields.bufOvfl is enabled.", reference)
  }
  if (payload.dataSetRef != null && candidate.dataSetRef != null && payload.dataSetRef !== candidate.dataSetRef) {
    pushDiagnostic(diagnostics, "error", "DATASET_MISMATCH", `Report DataSet reference "${payload.dataSetRef}" does not match SCD "${candidate.dataSetRef}".`, reference)
  }
  if (payload.confRev != null && candidate.confRev != null && payload.confRev !== candidate.confRev) {
    pushDiagnostic(diagnostics, "error", "CONFREV_MISMATCH", `Report ConfRev "${payload.confRev}" does not match SCD "${candidate.confRev}".`, reference)
  }
}

function mapPayloadValues(
  input: NormalizeIec61850ReportEventInput,
  reference: Iec61850ReportControlRef,
  reportReason: Iec61850ReportReason,
  diagnostics: Iec61850ReportEventDiagnostic[],
): Iec61850ReportValue[] {
  const { candidate, payload, receivedAt } = input
  const expectedSignals = getIec61850ReportCandidateSignals(candidate)
  const hasDataReferences = payload.values.some(value => value.dataReference != null && value.dataReference.trim() !== "")
  const assigned: AssignedValue[] = []

  const shouldExpectFullDataSet = reportReason === "general-interrogation" || reportReason === "integrity"
  if (payload.values.length !== expectedSignals.length && (!hasDataReferences || shouldExpectFullDataSet || payload.values.length > expectedSignals.length)) {
    pushDiagnostic(
      diagnostics,
      "warning",
      "VALUE_COUNT_MISMATCH",
      `Report payload contains ${payload.values.length} values but SCD DataSet contains ${expectedSignals.length} members.`,
      reference,
    )
  }

  if (hasDataReferences) {
    const index = buildReferenceIndex(candidate)
    const usedIndexes = new Set<number>()
    payload.values.forEach((payloadValue, payloadIndex) => {
      const dataReference = payloadValue.dataReference?.trim() ?? ""
      if (!dataReference) {
        const positionalIndex = payloadIndex < expectedSignals.length ? payloadIndex : -1
        if (positionalIndex >= 0 && !usedIndexes.has(positionalIndex)) {
          usedIndexes.add(positionalIndex)
          assigned.push({ index: positionalIndex, payloadValue })
        }
        pushDiagnostic(diagnostics, "warning", "MISSING_DATA_REFERENCE", "Report value is missing a data reference.", reference, payloadIndex, null)
        return
      }

      const matches = index.get(normalizeReportDataReference(dataReference, candidate)) ?? []
      if (matches.length === 0) {
        pushDiagnostic(diagnostics, "warning", "UNKNOWN_DATA_REFERENCE", `Report value data reference "${dataReference}" is not present in the SCD DataSet.`, reference, payloadIndex, dataReference)
        return
      }
      if (matches.length > 1) {
        pushDiagnostic(diagnostics, "warning", "DATA_REFERENCE_AMBIGUOUS", `Report value data reference "${dataReference}" matches multiple SCD DataSet members.`, reference, payloadIndex, dataReference)
        return
      }

      const dataSetIndex = matches[0] ?? -1
      if (usedIndexes.has(dataSetIndex)) {
        pushDiagnostic(diagnostics, "warning", "DUPLICATE_DATA_REFERENCE", `Report value data reference "${dataReference}" appears more than once in one report.`, reference, payloadIndex, dataReference)
        return
      }
      usedIndexes.add(dataSetIndex)
      assigned.push({ index: dataSetIndex, payloadValue })
    })
  } else {
    payload.values.forEach((payloadValue, payloadIndex) => {
      if (payloadIndex >= expectedSignals.length) {
        return
      }
      assigned.push({ index: payloadIndex, payloadValue })
    })
  }

  return assigned
    .sort((left, right) => left.index - right.index)
    .map(({ index, payloadValue }) => ({
      dataSetIndex: index,
      reference: expectedSignals[index]?.reference ?? payloadValue.dataReference ?? "",
      dataReference: payloadValue.dataReference ?? null,
      value: payloadValue.value,
      reasonCode: payloadValue.reasonCode ?? reportReason,
      timestamp: payloadValue.timestamp ?? input.payload.timeOfEntry ?? receivedAt,
    }))
}

function buildReferenceIndex(candidate: Iec61850ReportControlCandidate): ReferenceIndex {
  const index: ReferenceIndex = new Map()
  getIec61850ReportCandidateSignals(candidate).forEach((signal, signalIndex) => {
    for (const variant of buildReferenceVariants(signal.reference, candidate)) {
      const key = normalizeReportDataReference(variant, candidate)
      const existing = index.get(key)
      if (existing) {
        if (!existing.includes(signalIndex)) {
          existing.push(signalIndex)
        }
      } else {
        index.set(key, [signalIndex])
      }
    }
  })
  return index
}

function buildReferenceVariants(reference: string, candidate: Iec61850ReportControlCandidate): string[] {
  const variants = new Set<string>()
  variants.add(reference)
  variants.add(`${candidate.iedName}${reference}`)

  const fc = extractFunctionalConstraint(reference)
  const withoutFc = removeFunctionalConstraint(reference)
  variants.add(withoutFc)
  variants.add(`${candidate.iedName}${withoutFc}`)

  const slashReference = dotReferenceToSlashReference(reference)
  variants.add(slashReference)
  variants.add(`${candidate.iedName}${slashReference}`)

  if (fc) {
    const mmsReference = dotReferenceToMmsReference(reference, fc)
    variants.add(mmsReference)
    variants.add(`${candidate.iedName}${mmsReference}`)
  }

  return [...variants]
}

export function normalizeReportDataReference(
  dataReference: string,
  candidate: Iec61850ReportControlCandidate,
): string {
  let value = dataReference.trim()
  if (!value) {
    return ""
  }
  const bangIndex = value.indexOf("!")
  if (bangIndex >= 0) {
    value = value.slice(bangIndex + 1)
  }
  if (value.startsWith(candidate.iedName + candidate.logicalDeviceInst)) {
    value = value.slice(candidate.iedName.length)
  }
  value = mmsReferenceToDotReference(value)
  value = slashReferenceToDotReference(value)
  return value
}

function mmsReferenceToDotReference(reference: string): string {
  if (!reference.includes("$")) {
    return reference
  }
  const [logicalNodeRef, fc, ...path] = reference.split("$")
  if (!logicalNodeRef || !fc || path.length === 0) {
    return reference.replace(/\$/g, ".")
  }
  return `${logicalNodeRef}.${path.join(".")}[${fc}]`
}

function slashReferenceToDotReference(reference: string): string {
  const fc = extractFunctionalConstraint(reference)
  const withoutFc = removeFunctionalConstraint(reference)
  const parts = withoutFc.split("/").filter(Boolean)
  if (parts.length < 3) {
    return reference
  }
  const [ldInst, logicalNodeName, ...dataPath] = parts
  return `${ldInst}/${logicalNodeName}.${dataPath.join(".")}${fc ? `[${fc}]` : ""}`
}

function dotReferenceToSlashReference(reference: string): string {
  const fc = extractFunctionalConstraint(reference)
  const withoutFc = removeFunctionalConstraint(reference)
  const [logicalNodeRef, ...dataPath] = withoutFc.split(".")
  if (!logicalNodeRef || dataPath.length === 0) {
    return reference
  }
  return `${logicalNodeRef}/${dataPath.join("/")}${fc ? `[${fc}]` : ""}`
}

function dotReferenceToMmsReference(reference: string, fc: string): string {
  const withoutFc = removeFunctionalConstraint(reference)
  const [logicalNodeRef, ...dataPath] = withoutFc.split(".")
  if (!logicalNodeRef || dataPath.length === 0) {
    return reference
  }
  return `${logicalNodeRef}$${fc}$${dataPath.join("$")}`
}

function extractFunctionalConstraint(reference: string): string | null {
  const match = reference.match(/\[([A-Za-z0-9_]+)\]$/)
  return match?.[1] ?? null
}

function removeFunctionalConstraint(reference: string): string {
  return reference.replace(/\[[A-Za-z0-9_]+\]$/, "")
}

function resolveReportReason(payload: Iec61850ReportPayload): Iec61850ReportReason {
  return payload.reason ?? payload.values.find(value => value.reasonCode != null)?.reasonCode ?? "data-change"
}

function buildReportEventId(
  endpointId: string,
  reference: Iec61850ReportControlRef,
  sequenceNumber: number | null,
  receivedAt: string,
): string {
  return [
    endpointId,
    reference.iedName,
    reference.accessPointName,
    reference.logicalDeviceInst,
    reference.logicalNodeName,
    reference.reportControlName,
    sequenceNumber ?? "no-seq",
    receivedAt,
  ].join(":")
}

function pushDiagnostic(
  diagnostics: Iec61850ReportEventDiagnostic[],
  severity: Iec61850ReportEventDiagnostic["severity"],
  code: Iec61850ReportEventDiagnosticCode,
  message: string,
  reference: Iec61850ReportControlRef,
  valueIndex?: number,
  dataReference?: string | null,
) {
  diagnostics.push({
    severity,
    code,
    message,
    reference,
    valueIndex,
    dataReference,
  })
}
