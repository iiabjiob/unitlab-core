import type {
  Iec61850ReportSubscriptionCandidate,
  NormalizedDataLeaf,
  SclDataSetMember,
  SclReportOptionalFields,
  SclReportTriggerOptions,
} from "../scd-sld-core"

export type Iec61850RuntimeMode = "simulator" | "mms"

export type Iec61850DeviceEndpoint = {
  id: string
  mode: Iec61850RuntimeMode
  iedName: string
  accessPointName: string
  host: string | null
  port: number
}

export type Iec61850ReportControlRef = {
  iedName: string
  accessPointName: string
  logicalDeviceInst: string
  logicalNodeName: string
  reportControlName: string
  reportKind: "buffered" | "unbuffered"
}

export type Iec61850ReportLifecycleState =
  | "disconnected"
  | "connected"
  | "read"
  | "reserved"
  | "enabled"
  | "gi-pending"
  | "reporting"
  | "disabled"
  | "released"
  | "failed"

export type Iec61850ReportControlState = {
  reference: Iec61850ReportControlRef
  lifecycleState: Iec61850ReportLifecycleState
  rptId: string | null
  dataSetRef: string | null
  confRev: string | null
  indexed: boolean | null
  bufferTimeMs: number | null
  integrityPeriodMs: number | null
  triggerOptions: SclReportTriggerOptions
  optionalFields: SclReportOptionalFields
  signalCount: number
  enabled: boolean
  reservedBy: string | null
  owner: string | null
  sequenceNumber: number
  giInProgress: boolean
}

export type Iec61850ReportRuntimeDiagnostic = {
  severity: "error" | "warning" | "info"
  code: string
  message: string
  reference: Iec61850ReportControlRef
}

export type Iec61850ReportControlReadResult = {
  endpoint: Iec61850DeviceEndpoint
  candidateId: string
  state: Iec61850ReportControlState
  diagnostics: Iec61850ReportRuntimeDiagnostic[]
}

export type Iec61850SelectedSignal = {
  id: string
  address: string
  label?: string | null
}

export type Iec61850ReportSubscriptionPlanDiagnostic = {
  severity: "error" | "warning" | "info"
  code:
    | "SIGNAL_NOT_FOUND"
    | "SIGNAL_AMBIGUOUS"
    | "DUPLICATE_SELECTED_SIGNAL"
    | "FCD_PARENT_MATCH"
    | "MULTIPLE_REPORT_CANDIDATES"
  message: string
  signalId?: string
  address?: string
}

export type Iec61850ReportSubscriptionPlanSignal = {
  selectedSignal: Iec61850SelectedSignal
  modelReference: string
  iedName: string
  matchKind: "exact" | "fcd-parent"
}

export type Iec61850ReportSubscriptionPlanReport = {
  status: "required"
  candidate: Iec61850ReportControlCandidate
  matchedSignals: Iec61850ReportSubscriptionPlanSignal[]
}

export type Iec61850ReportSubscriptionPlanDevice = {
  iedName: string
  accessPointName: string
  reports: Iec61850ReportSubscriptionPlanReport[]
}

export type Iec61850ReportSubscriptionPlan = {
  selectedSignalCount: number
  matchedSignalCount: number
  unmatchedSignalCount: number
  ambiguousSignalCount: number
  requiredReportCount: number
  devices: Iec61850ReportSubscriptionPlanDevice[]
  matchedSignals: Iec61850ReportSubscriptionPlanSignal[]
  unmatchedSignals: Iec61850SelectedSignal[]
  ambiguousSignals: Array<{
    selectedSignal: Iec61850SelectedSignal
    candidates: Array<{
      iedName: string
      reference: string
      reportCandidateIds: string[]
    }>
  }>
  diagnostics: Iec61850ReportSubscriptionPlanDiagnostic[]
}

export type Iec61850ReportReason =
  | "general-interrogation"
  | "data-change"
  | "quality-change"
  | "data-update"
  | "integrity"

export type Iec61850ReportJsonValue =
  | boolean
  | number
  | string
  | null
  | Iec61850ReportJsonValue[]
  | { [key: string]: Iec61850ReportJsonValue }

export type Iec61850ReportValue = {
  dataSetIndex: number
  reference: string
  dataReference: string | null
  value: Iec61850ReportJsonValue
  reasonCode: Iec61850ReportReason
  timestamp: string
}

export type Iec61850ReportEvent = {
  id: string
  endpointId: string
  receivedAt: string
  reportControl: Iec61850ReportControlRef
  rptId: string | null
  dataSetRef: string | null
  confRev: string | null
  sequenceNumber: number | null
  timeOfEntry: string | null
  entryId: string | null
  bufferOverflow: boolean | null
  reason: Iec61850ReportReason
  values: Iec61850ReportValue[]
}

export type Iec61850ReportConnection = {
  readReportControl(reference: Iec61850ReportControlRef): Promise<Iec61850ReportControlState>
  reserveReportControl(reference: Iec61850ReportControlRef, clientId: string): Promise<Iec61850ReportControlState>
  releaseReportControl(reference: Iec61850ReportControlRef, clientId: string): Promise<Iec61850ReportControlState>
  enableReportControl(reference: Iec61850ReportControlRef, clientId: string): Promise<Iec61850ReportControlState>
  disableReportControl(reference: Iec61850ReportControlRef, clientId: string): Promise<Iec61850ReportControlState>
  sendGeneralInterrogation(reference: Iec61850ReportControlRef, clientId: string): Promise<Iec61850ReportEvent>
  disconnect(): Promise<void>
}

export type Iec61850ReportManagerAdapter = {
  connect(endpoint: Iec61850DeviceEndpoint): Promise<Iec61850ReportConnection>
}

export type Iec61850ReportManagerReadOnlyAdapter = Pick<Iec61850ReportManagerAdapter, "connect">

export type Iec61850ReportControlCandidate = Iec61850ReportSubscriptionCandidate

export type Iec61850ReportSignal = NormalizedDataLeaf
