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

export type Iec61850NativeNormalizedModel = {
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
