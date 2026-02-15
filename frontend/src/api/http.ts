import axios from "axios"

export const http = axios.create({
  timeout: 30000,
  headers: {
    "Content-Type": "application/json",
  },
})

type RetryableConfig = {
  __retryCount?: number
  __retryDelayMs?: number
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
    if (!isRetryableTransportError(error) || retries >= 1) {
      return Promise.reject(error)
    }

    config.__retryCount = retries + 1
    const delayMs = Math.max(50, Number(config.__retryDelayMs ?? 180))
    await new Promise(resolve => setTimeout(resolve, delayMs))
    return http.request(config)
  },
)
