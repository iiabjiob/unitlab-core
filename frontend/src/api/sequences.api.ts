import { http } from "./http"
import { API_V1, buildQuery } from "./utils"
import type { SequenceStep, SequenceStepCreate } from "@/types/sequences"

const basePath = (workspaceId: number | string) => `${API_V1}/workspaces/${workspaceId}/sequences`

export const SequencesAPI = {
  list(workspaceId: number | string, params?: Record<string, any>) {
    return http.get(buildQuery(basePath(workspaceId), params))
  },

  get(workspaceId: number | string, id: number | string) {
    return http.get(`${basePath(workspaceId)}/${id}`)
  },

  create(workspaceId: number | string, payload: any) {
    return http.post(basePath(workspaceId), payload)
  },

  update(workspaceId: number | string, id: number | string, payload: any) {
    return http.patch(`${basePath(workspaceId)}/${id}`, payload)
  },

  delete(workspaceId: number | string, id: number | string) {
    return http.delete(`${basePath(workspaceId)}/${id}`)
  },

  // Steps
  getSteps(workspaceId: number | string, seqId: number | string) {
    return http.get<SequenceStep[]>(`${basePath(workspaceId)}/${seqId}/steps`)
  },

  addStep(workspaceId: number | string, seqId: number | string, payload: SequenceStepCreate) {
    return http.post<SequenceStep>(`${basePath(workspaceId)}/${seqId}/steps`, payload)
  },

  updateStep(
    workspaceId: number | string,
    seqId: number | string,
    stepId: number | string,
    payload: Partial<SequenceStep>,
  ) {
    return http.patch<SequenceStep>(`${basePath(workspaceId)}/${seqId}/steps/${stepId}`, payload)
  },

  deleteStep(workspaceId: number | string, seqId: number | string, stepId: number | string) {
    return http.delete(`${basePath(workspaceId)}/${seqId}/steps/${stepId}`)
  },

  reorderSteps(workspaceId: number | string, seqId: number | string, payload: { new_order: number[] }) {
    return http.post<SequenceStep[]>(`${basePath(workspaceId)}/${seqId}/steps/reorder`, payload)
  },

  replaceSteps(workspaceId: number | string, seqId: number | string, payload: SequenceStepCreate[]) {
    return http.put<SequenceStep[]>(`${basePath(workspaceId)}/${seqId}/steps`, payload)
  },

  // Execution
  start(workspaceId: number | string, seqId: number | string) {
    return http.post(`${basePath(workspaceId)}/${seqId}/start`)
  },

  stop(workspaceId: number | string, seqId: number | string) {
    return http.post(`${basePath(workspaceId)}/${seqId}/stop`)
  },

  getState(workspaceId: number | string, seqId: number | string) {
    return http.get(`${basePath(workspaceId)}/${seqId}/state`)
  },

  export(workspaceId: number | string, seqId: number | string) {
    return http.get(`${basePath(workspaceId)}/${seqId}/export-file`)
  },

  import(workspaceId: number | string, formData: FormData) {
    return http.post(`${basePath(workspaceId)}/import-file`, formData, {
      headers: { "Content-Type": "multipart/form-data" },
    })
  },
}
