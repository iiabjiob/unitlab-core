import type {
  Iec61850ReportSubscriptionCandidate,
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

export type Iec61850ReportControlState = {
  reference: Iec61850ReportControlRef
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

export type Iec61850ReportValue = {
  reference: string
  value: boolean | number | string | null
  reasonCode: "general-interrogation" | "data-change" | "quality-change" | "data-update" | "integrity"
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
  sequenceNumber: number
  reason: "general-interrogation" | "data-change" | "quality-change" | "data-update" | "integrity"
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

export type Iec61850ReportSignal = SclDataSetMember
