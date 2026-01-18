import { http } from "./http"
import { API_V1 } from "./utils"
import type { SystemHealthResponse } from "@/types/health"

export async function fetchSystemHealth() {
  const { data } = await http.get<SystemHealthResponse>(`${API_V1}/health`)
  return data
}
