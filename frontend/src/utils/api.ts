
import axios, { type AxiosInstance } from "axios"

export const api: AxiosInstance = axios.create({
  timeout: 10000,
  headers: {
    "Content-Type": "application/json",
  },
})

function buildQuery(baseUrl: string, params?: Record<string, any>) {
  if (!params) return baseUrl

  const queryString = Object.entries(params)
    .filter(([, value]) => value !== undefined && value !== null)
    .map(([key, value]) => `${encodeURIComponent(key)}=${encodeURIComponent(value)}`)
    .join('&')

  return queryString ? `${baseUrl}?${queryString}` : baseUrl
}

const API_V1 = "/api/v1"

export const ApiBuilder = {
  devices: (params?: Record<string, any>) => buildQuery(`${API_V1}/devices`, params),
  device: (id: number) => `${API_V1}/devices/${id}`,
  devicesBulkDelete: () => `${API_V1}/devices/bulk`,
  deviceChannels: (id: number, params?: Record<string, any>) =>
    buildQuery(`${API_V1}/devices/${id}/channels`, params),

  channels: (params?: Record<string, any>) => buildQuery(`${API_V1}/channels`, params),
  channel: (id: number) => `${API_V1}/channels/${id}`,

  switchgears: () => "/api/switchgears",
  switchgear: (id: number) => `/api/switchgears/${id}`,

  sequences: () => `${API_V1}/sequences`,
  sequence: (id: number) => `${API_V1}/sequences/${id}`,
  sequenceExport: (id: number) => `${API_V1}/sequences/${id}/export-file`,
  sequenceImport: () => `${API_V1}/sequences/import-file`,
  sequenceSteps: (seqId: number) => `${API_V1}/sequences/${seqId}/steps`,
  sequenceStep: (seqId: number, stepId: number) => `${API_V1}/sequences/${seqId}/steps/${stepId}`,
  sequenceStepsReorder: (seqId: number) => `${API_V1}/sequences/${seqId}/steps/reorder`,
  sequenceStepsReplace: (seqId: number) => `${API_V1}/sequences/${seqId}/steps`,
  sequenceState: (seqId: number) => `${API_V1}/sequences/${seqId}/state`,
  sequenceStart: (seqId: number) => `${API_V1}/sequences/${seqId}/start`,
  sequenceStop: (seqId: number) => `${API_V1}/sequences/${seqId}/stop`,

  events: (params?: Record<string, any>) => buildQuery('/api/events', params),

  time: (params?: Record<string, any>) => buildQuery('/api/time', params),

  timeSync: () => `/api/settings/timesync`,
}
