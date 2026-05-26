import { httpData } from "./http"
import { API_V1 } from "./utils"
import type { SystemHealthResponse } from "@/types/health"

export async function fetchSystemHealth() {
  return httpData.get<SystemHealthResponse>(`${API_V1}/health`)
}
