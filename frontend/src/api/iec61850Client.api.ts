import { httpData } from "./http"
import { API_V1 } from "./utils"

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
}
