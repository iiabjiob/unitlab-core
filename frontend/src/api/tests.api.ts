import { http } from "./http"
import { API_V1, buildQuery } from "./utils"
import type {
  TestRunCreatePayload,
  TestRunSchema,
  TestRunState,
  TestRunStep,
  TestRunSummary,
  TestRunUpdatePayload,
} from "@/types/testRuns"

const basePath = (projectId: number | string) => `${API_V1}/projects/${projectId}/tests`

export const TestsAPI = {
  list(projectId: number | string, params?: Record<string, any>) {
    return http.get<TestRunSummary[]>(buildQuery(basePath(projectId), params))
  },

  get(projectId: number | string, id: number | string) {
    return http.get<TestRunSchema>(`${basePath(projectId)}/${id}`)
  },

  create(projectId: number | string, payload: TestRunCreatePayload) {
    return http.post<TestRunSchema>(basePath(projectId), payload)
  },

  update(projectId: number | string, id: number | string, payload: TestRunUpdatePayload) {
    return http.patch<TestRunSchema>(`${basePath(projectId)}/${id}`, payload)
  },

  delete(projectId: number | string, id: number | string) {
    return http.delete(`${basePath(projectId)}/${id}`)
  },

  // Steps
  getSteps(projectId: number | string, runId: number | string) {
    return http.get<TestRunStep[]>(`${basePath(projectId)}/${runId}/steps`)
  },

  addStep(projectId: number | string, runId: number | string, payload: { channel_id: number }) {
    return http.post<TestRunStep>(`${basePath(projectId)}/${runId}/steps`, payload)
  },

  bulkAddSteps(projectId: number | string, runId: number | string, payload: { channel_ids: number[] }) {
    return http.post<TestRunStep[]>(`${basePath(projectId)}/${runId}/steps/bulk`, payload)
  },

  updateStep(
    projectId: number | string,
    runId: number | string,
    stepId: number | string,
    payload: Partial<TestRunStep>,
  ) {
    return http.patch<TestRunStep>(`${basePath(projectId)}/${runId}/steps/${stepId}`, payload)
  },

  deleteStep(projectId: number | string, runId: number | string, stepId: number | string) {
    return http.delete(`${basePath(projectId)}/${runId}/steps/${stepId}`)
  },

  reorderSteps(projectId: number | string, runId: number | string, payload: { new_order: number[] }) {
    return http.post<TestRunStep[]>(`${basePath(projectId)}/${runId}/steps/reorder`, payload)
  },

  // Execution
  start(projectId: number | string, runId: number | string) {
    return http.post<TestRunSchema>(`${basePath(projectId)}/${runId}/start`)
  },

  cancel(projectId: number | string, runId: number | string) {
    return http.post<TestRunSchema>(`${basePath(projectId)}/${runId}/cancel`)
  },

  state(projectId: number | string, runId: number | string) {
    return http.get<TestRunState>(`${basePath(projectId)}/${runId}/state`)
  },
}
