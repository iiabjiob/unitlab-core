import { http } from "./http"
import { API_V1 } from "./utils"
import type { Signal, SignalCreatePayload, SignalUpdatePayload } from "@/types/signal"

export const SignalsAPI = {
  list(workspaceId: number) {
    return http.get<Signal[]>(`${API_V1}/workspaces/${workspaceId}/signals`)
  },

  create(workspaceId: number, payload: SignalCreatePayload) {
    return http.post<Signal>(`${API_V1}/workspaces/${workspaceId}/signals`, payload)
  },

  get(signalId: number) {
    return http.get<Signal>(`${API_V1}/signals/${signalId}`)
  },

  update(signalId: number, payload: SignalUpdatePayload) {
    return http.put<Signal>(`${API_V1}/signals/${signalId}`, payload)
  },

  delete(signalId: number) {
    return http.delete<void>(`${API_V1}/signals/${signalId}`)
  },
}
