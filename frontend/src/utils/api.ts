
function buildQuery(baseUrl: string, params?: Record<string, any>) {
  if (!params) return baseUrl

  const queryString = Object.entries(params)
    .filter(([, value]) => value !== undefined && value !== null)
    .map(([key, value]) => `${encodeURIComponent(key)}=${encodeURIComponent(value)}`)
    .join('&')

  return queryString ? `${baseUrl}?${queryString}` : baseUrl
}

export const ApiBuilder = {
  devices: (params?: Record<string, any>) => buildQuery('/api/devices', params),
  device: (id: number) => `/api/devices/${id}`,

  channel: (id: number) => `/api/channels/${id}`,

  switchgears: () => "/api/switchgears",
  switchgear: (id: number) => `/api/switchgears/${id}`,

  sequences: () => `/api/sequences`,
  sequence: (id: number) => `/api/sequences/${id}`,
  sequenceExport: (id: number) => `/api/sequences/${id}/export-file`,
  sequenceImport: () => `/api/sequences/import-file`,
  sequenceSteps: (seqId: number) => `/api/sequences/${seqId}/steps`,
  sequenceStep: (seqId: number, stepId: number) => `/api/sequences/${seqId}/steps/${stepId}`,
  sequenceStepsReorder: (seqId: number) => `/api/sequences/${seqId}/steps/reorder`,

  events: (params?: Record<string, any>) => buildQuery('/api/events', params),

  time: (params?: Record<string, any>) => buildQuery('/api/time', params),

  timeSync: () => `/api/settings/timesync`,
}
