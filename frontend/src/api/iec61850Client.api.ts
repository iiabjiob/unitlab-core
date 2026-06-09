import { httpData } from "./http"
import { API_V1 } from "./utils"


export type Iec61850SclDiagnostic = {
  severity: string
  code: string
  message: string
  iedName: string
  accessPointName: string
  logicalDeviceInst: string
  logicalNodeName: string
  dataSetName: string
  reportControlName: string
  memberReference: string
}

export type Iec61850SclIedSummary = {
  name: string
  accessPointCount: number
}

export type Iec61850SclIedDiscoveryResponse = {
  schema: string
  sourceSize: number
  ieds: Iec61850SclIedSummary[]
  diagnostics: Iec61850SclDiagnostic[]
}

export type Iec61850NativeNormalizedModel = {
  network?: Record<string, unknown>
  logicalDevices?: Array<Record<string, unknown>>
  logicalNodes?: Array<Record<string, unknown>>
  dataSets?: Array<Record<string, unknown>>
  reports?: Array<Record<string, unknown>>
  signals?: Array<Record<string, unknown>>
}

export type Iec61850SclImportResponse = {
  import_id: string
  workspace_id: number
  source_filename: string | null
  source_hash: string
  source_size: number
  selected_ied: string
  normalized_schema: string
  normalized_model: Iec61850NativeNormalizedModel
  diagnostics: Iec61850SclDiagnostic[]
}

export type Iec61850SclImportFailure = {
  selected_ied: string
  code: string
  message: string
}

export type Iec61850SclImportBatchResponse = {
  workspace_id: number
  source_filename: string | null
  source_size: number
  imports: Iec61850SclImportResponse[]
  failures: Iec61850SclImportFailure[]
}

export type Iec61850SclImportListResponse = {
  workspace_id: number
  imports: Iec61850SclImportResponse[]
}

export type Iec61850SclImportBatchJobStartResponse = {
  job_id: string
  total: number
}

export type Iec61850SclImportBatchJobStatus = {
  job_id: string
  status: "queued" | "running" | "completed" | "failed" | "cancelled" | string
  workspace_id: number
  source_filename: string | null
  total: number
  current: number
  current_ied: string | null
  imports: Iec61850SclImportResponse[]
  failures: Iec61850SclImportFailure[]
  message: string | null
}

export type Iec61850VirtualMmsServerState = {
  running: boolean
  import_id: string | null
  selected_ied: string | null
  source_hash: string | null
  host: string | null
  port: number | null
  pid: number | null
  fixture_path: string | null
  binary_path: string | null
  message: string | null
}

export type Iec61850RuntimeSelectionResponse = {
  selection_id: string
  workspace_id: number
  import_id: string
  runtime_revision: number
  selected_ied: string
  source_hash: string
  normalized_schema: string
  selected_by: string | null
  selection_reason: string | null
}

export type Iec61850ClientDiagnostic = {
  action: string
  code: string
  message: string
}

export type Iec61850ClientEvent = {
  id: string
  at: string
  kind: string
  session_id: string
  endpoint_id: string
  candidate_id: string | null
  report_control_name: string | null
  client_id: string | null
  outcome: string | null
  code: string | null
  message: string | null
}

export type Iec61850ClientState = {
  session_id: string
  client_id: string
  session_open: boolean
  endpoint: {
    id: string
    mode: string
    ied_name: string
    access_point_name: string
    host: string | null
    port: number
  }
  candidate: {
    id: string
    ied_name: string
    access_point_name: string
    logical_device_inst: string
    logical_node_name: string
    report_control_name: string
    report_kind: string
    rpt_id: string | null
    data_set_ref: string | null
    conf_rev: string | null
    indexed: boolean | null
    buffer_time_ms: number | null
    integrity_period_ms: number | null
  }
  last_read: Record<string, unknown> | null
  last_state: Record<string, unknown> | null
  last_report: Record<string, unknown> | null
  last_plan: Record<string, unknown> | null
  transcript: Iec61850ClientEvent[]
  last_diagnostic: Iec61850ClientDiagnostic | null
  live_wire_open: boolean
  live_wire_endpoint: {
    id: string
    mode: string
    ied_name: string
    access_point_name: string
    host: string | null
    port: number
  } | null
  live_wire_last_frame_length: number | null
  live_wire_last_frame_hex: string | null
  live_wire_last_diagnostic: Iec61850ClientDiagnostic | null
}

export const Iec61850SclAPI = {
  listSclImports(workspaceId: number, limit = 100) {
    return httpData.get<Iec61850SclImportListResponse>(`${API_V1}/workspaces/${workspaceId}/iec61850/scl/imports`, { params: { limit } })
  },

  discoverIeds(workspaceId: number, file: File) {
    const form = new FormData()
    form.append("file", file)
    return httpData.post<Iec61850SclIedDiscoveryResponse>(`${API_V1}/workspaces/${workspaceId}/iec61850/scl/ieds`, form, {
      timeout: 300000,
    })
  },

  startSclImportBatchJob(workspaceId: number, file: File, selectedIeds: string[]) {
    const form = new FormData()
    form.append("file", file)
    form.append("selected_ieds", JSON.stringify(selectedIeds))
    return httpData.post<Iec61850SclImportBatchJobStartResponse>(`${API_V1}/workspaces/${workspaceId}/iec61850/scl/import-batch/jobs`, form, {
      timeout: 120000,
    })
  },

  getSclImportBatchJob(workspaceId: number, jobId: string) {
    return httpData.get<Iec61850SclImportBatchJobStatus>(`${API_V1}/workspaces/${workspaceId}/iec61850/scl/import-batch/jobs/${jobId}`)
  },

  cancelSclImportBatchJob(workspaceId: number, jobId: string) {
    return httpData.post<Iec61850SclImportBatchJobStatus>(`${API_V1}/workspaces/${workspaceId}/iec61850/scl/import-batch/jobs/${jobId}/cancel`)
  },

  importSclBatch(workspaceId: number, file: File, selectedIeds: string[], signal?: AbortSignal) {
    const form = new FormData()
    form.append("file", file)
    form.append("selected_ieds", JSON.stringify(selectedIeds))
    return httpData.post<Iec61850SclImportBatchResponse>(`${API_V1}/workspaces/${workspaceId}/iec61850/scl/import-batch`, form, {
      timeout: 600000,
      signal,
    })
  },

  importScl(workspaceId: number, file: File, selectedIed?: string) {
    const form = new FormData()
    form.append("file", file)
    const normalizedSelectedIed = selectedIed?.trim()
    if (normalizedSelectedIed) {
      form.append("selected_ied", normalizedSelectedIed)
    }
    return httpData.post<Iec61850SclImportResponse>(`${API_V1}/workspaces/${workspaceId}/iec61850/scl/import`, form, {
      timeout: 120000,
    })
  },

  virtualMmsServerState(workspaceId: number) {
    return httpData.get<Iec61850VirtualMmsServerState>(`${API_V1}/workspaces/${workspaceId}/iec61850/virtual-mms-server`)
  },

  startVirtualMmsServer(workspaceId: number, payload: { import_id: string; host?: string; port?: number }) {
    return httpData.post<Iec61850VirtualMmsServerState>(`${API_V1}/workspaces/${workspaceId}/iec61850/virtual-mms-server/start`, payload, { timeout: 30000 })
  },

  stopVirtualMmsServer(workspaceId: number) {
    return httpData.post<Iec61850VirtualMmsServerState>(`${API_V1}/workspaces/${workspaceId}/iec61850/virtual-mms-server/stop`, undefined, { timeout: 10000 })
  },

  runtimeSelection(workspaceId: number) {
    return httpData.get<Iec61850RuntimeSelectionResponse | null>(`${API_V1}/workspaces/${workspaceId}/iec61850/runtime/selection`)
  },

  selectRuntimeImport(workspaceId: number, payload: { import_id: string; selected_by?: string | null; reason?: string | null }) {
    return httpData.post<Iec61850RuntimeSelectionResponse>(`${API_V1}/workspaces/${workspaceId}/iec61850/runtime/selection`, payload)
  },
}

export const Iec61850ClientAPI = {
  state() {
    return httpData.get<Iec61850ClientState>(`${API_V1}/iec61850/client/state`)
  },

  transcript() {
    return httpData.get<{ transcript: Iec61850ClientEvent[] }>(`${API_V1}/iec61850/client/transcript`)
  },

  clearTranscript() {
    return httpData.post<Iec61850ClientState>(`${API_V1}/iec61850/client/transcript/clear`)
  },

  openSession() {
    return httpData.post<Iec61850ClientState>(`${API_V1}/iec61850/client/session/open`)
  },

  closeSession() {
    return httpData.post<Iec61850ClientState>(`${API_V1}/iec61850/client/session/close`)
  },

  readReportControl() {
    return httpData.post<Iec61850ClientState>(`${API_V1}/iec61850/client/report-control/read`)
  },

  reserveReportControl() {
    return httpData.post<Iec61850ClientState>(`${API_V1}/iec61850/client/report-control/reserve`)
  },

  enableReportControl() {
    return httpData.post<Iec61850ClientState>(`${API_V1}/iec61850/client/report-control/enable`)
  },

  sendGeneralInterrogation() {
    return httpData.post<Iec61850ClientState>(`${API_V1}/iec61850/client/report-control/gi`)
  },

  disableReportControl() {
    return httpData.post<Iec61850ClientState>(`${API_V1}/iec61850/client/report-control/disable`)
  },

  releaseReportControl() {
    return httpData.post<Iec61850ClientState>(`${API_V1}/iec61850/client/report-control/release`)
  },

  runSubscriptionPlan() {
    return httpData.post<Iec61850ClientState>(`${API_V1}/iec61850/client/subscription/run`)
  },

  startWireTransport() {
    return httpData.post<Iec61850ClientState>(`${API_V1}/iec61850/client/wire/start`)
  },

  emitWireReport() {
    return httpData.post<Iec61850ClientState>(`${API_V1}/iec61850/client/wire/emit-report`)
  },

  stopWireTransport() {
    return httpData.post<Iec61850ClientState>(`${API_V1}/iec61850/client/wire/stop`)
  },
}
