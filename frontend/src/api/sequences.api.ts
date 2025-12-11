import { http } from "./http"
import { API_V1, buildQuery } from "./utils"
import type { SequenceStep, SequenceStepCreate } from "@/types/sequences"

export const SequencesAPI = {
  list(params?: Record<string, any>) {
    return http.get(buildQuery(`${API_V1}/sequences`, params))
  },

  get(id: number | string) {
    return http.get(`${API_V1}/sequences/${id}`)
  },

  create(payload: any) {
    return http.post(`${API_V1}/sequences`, payload)
  },

  update(id: number | string, payload: any) {
    return http.patch(`${API_V1}/sequences/${id}`, payload)
  },

  delete(id: number | string) {
    return http.delete(`${API_V1}/sequences/${id}`)
  },

  // Steps
  getSteps(seqId: number | string) {
    return http.get<SequenceStep[]>(`${API_V1}/sequences/${seqId}/steps`)
  },

  addStep(seqId: number | string, payload: SequenceStepCreate) {
    return http.post<SequenceStep>(`${API_V1}/sequences/${seqId}/steps`, payload)
  },

  updateStep(seqId: number | string, stepId: number | string, payload: Partial<SequenceStep>) {
    return http.patch<SequenceStep>(`${API_V1}/sequences/${seqId}/steps/${stepId}`, payload)
  },

  deleteStep(seqId: number | string, stepId: number | string) {
    return http.delete(`${API_V1}/sequences/${seqId}/steps/${stepId}`)
  },

  reorderSteps(seqId: number | string, payload: { new_order: number[] }) {
    return http.post<SequenceStep[]>(`${API_V1}/sequences/${seqId}/steps/reorder`, payload)
  },

  replaceSteps(seqId: number | string, payload: SequenceStepCreate[]) {
    return http.put<SequenceStep[]>(`${API_V1}/sequences/${seqId}/steps`, payload)
  },

  // Execution
  start(seqId: number | string) {
    return http.post(`${API_V1}/sequences/${seqId}/start`)
  },

  stop(seqId: number | string) {
    return http.post(`${API_V1}/sequences/${seqId}/stop`)
  },

  getState(seqId: number | string) {
    return http.get(`${API_V1}/sequences/${seqId}/state`)
  },

  export(seqId: number | string) {
    return http.get(`${API_V1}/sequences/${seqId}/export-file`)
  },

  import(formData: FormData) {
    return http.post(`${API_V1}/sequences/import-file`, formData)
  },
}
