import axios from "axios"
import { toUserFacingErrorMessage } from "@/api/errorMessages"

export const http = axios.create({
  timeout: 30000,
  headers: {
    "Content-Type": "application/json",
  },
})

type RetryableConfig = {
  __retryCount?: number
  __retryDelayMs?: number
  params?: Record<string, unknown>
  url?: string
  method?: string
}

function isRetryableTransportError(error: unknown): boolean {
  if (!axios.isAxiosError(error)) return false
  const config = (error.config ?? {}) as RetryableConfig
  const method = String(config.method ?? "get").toLowerCase()
  if (method !== "get") return false

  const url = String(config.url ?? "")
  const isSignalDataReadEndpoint = /\/signal-(?:allocations|sheet)(?:\/|\?|$)/.test(url)
  const code = String(error.code ?? "")
  const message = String(error.message ?? "")
  const contentLengthMismatch = /content_length_mismatch|content-length/i.test(message)
  const networkLike = code === "ERR_NETWORK" || code === "ECONNRESET" || code === "ETIMEDOUT"

  return isSignalDataReadEndpoint && (contentLengthMismatch || networkLike)
}

http.interceptors.response.use(
  response => response,
  async (error) => {
    if (!axios.isAxiosError(error) || !error.config) {
      return Promise.reject(error)
    }

    const config = error.config as RetryableConfig
    const retries = Number(config.__retryCount ?? 0)
    const maxRetries = 3
    if (!isRetryableTransportError(error) || retries >= maxRetries) {
      error.message = toUserFacingErrorMessage(error, "Request failed")
      return Promise.reject(error)
    }

    config.__retryCount = retries + 1
    const baseDelayMs = Math.max(80, Number(config.__retryDelayMs ?? 160))
    const delayMs = baseDelayMs * (retries + 1)
    const params = { ...(config.params ?? {}) }
    params.__retryTs = Date.now()
    config.params = params

    await new Promise(resolve => setTimeout(resolve, delayMs))
    return http.request(config)
  },
)
