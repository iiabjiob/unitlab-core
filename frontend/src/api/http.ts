import axios, { type AxiosRequestConfig, type AxiosResponse } from "axios"
import { toUserFacingErrorMessage } from "@/api/errorMessages"
import {
  getHttpErrorContext,
  HttpRequestError,
  type HttpMethod,
} from "@/api/httpErrors"

export type HttpRequestOptions = {
  headers?: Record<string, string>
  params?: Record<string, unknown>
  data?: unknown
  timeout?: number
  signal?: AbortSignal
  responseType?: AxiosRequestConfig["responseType"]
}

export type HttpRequestConfig = HttpRequestOptions & {
  method: HttpMethod
  url: string
}

export type HttpResponse<T> = {
  data: T
  status: number
  statusText: string
  headers: Record<string, unknown>
  url: string
  method: string
}

export type HttpResult<T> =
  | { ok: true; data: T }
  | { ok: false; error: HttpRequestError; message: string; status: number | null }

type RetryableConfig = AxiosRequestConfig & {
  __retryCount?: number
  __retryDelayMs?: number
}

const axiosClient = axios.create({
  timeout: 30000,
  headers: {
    "Content-Type": "application/json",
  },
})

function isRetryableTransportError(error: unknown): boolean {
  if (!axios.isAxiosError(error)) return false
  const config = (error.config ?? {}) as RetryableConfig
  const method = String(config.method ?? "get").toLowerCase()
  if (method !== "get") return false

  const url = String(config.url ?? "")
  const isApiReadEndpoint = /\/api\/v\d+\//.test(url) || /^\/api\//.test(url)
  const code = String(error.code ?? "")
  const message = String(error.message ?? "")
  const contentLengthMismatch = /content_length_mismatch|content-length|length\s*mismatch/i.test(message)
  const networkLike = code === "ERR_NETWORK" || code === "ECONNRESET" || code === "ETIMEDOUT"

  return isApiReadEndpoint && (contentLengthMismatch || networkLike)
}

function toHttpRequestError(error: unknown, fallback = "Request failed"): HttpRequestError {
  if (error instanceof HttpRequestError) {
    return error
  }
  return new HttpRequestError(
    toUserFacingErrorMessage(error, fallback),
    getHttpErrorContext(error),
    { cause: error },
  )
}

function toHttpResponse<T>(response: AxiosResponse<T>): HttpResponse<T> {
  return {
    data: response.data,
    status: response.status,
    statusText: response.statusText,
    headers: response.headers as Record<string, unknown>,
    url: String(response.config.url ?? ""),
    method: String(response.config.method ?? "").toUpperCase(),
  }
}

axiosClient.interceptors.response.use(
  response => response,
  async (error) => {
    if (!axios.isAxiosError(error) || !error.config) {
      return Promise.reject(toHttpRequestError(error))
    }

    const config = error.config as RetryableConfig
    const retries = Number(config.__retryCount ?? 0)
    const maxRetries = 3
    if (!isRetryableTransportError(error) || retries >= maxRetries) {
      return Promise.reject(toHttpRequestError(error))
    }

    config.__retryCount = retries + 1
    const baseDelayMs = Math.max(80, Number(config.__retryDelayMs ?? 160))
    const delayMs = baseDelayMs * (retries + 1)
    const params = { ...(config.params as Record<string, unknown> | undefined) }
    params.__retryTs = Date.now()
    config.params = params

    await new Promise(resolve => setTimeout(resolve, delayMs))
    return axiosClient.request(config)
  },
)

async function request<T>(config: HttpRequestConfig): Promise<HttpResponse<T>> {
  const response = await axiosClient.request<T>({
    method: config.method,
    url: config.url,
    data: config.data,
    params: config.params,
    headers: config.headers,
    timeout: config.timeout,
    signal: config.signal,
    responseType: config.responseType,
  })
  return toHttpResponse(response)
}

async function requestData<T>(config: HttpRequestConfig): Promise<T> {
  const response = await request<T>(config)
  return response.data
}

export const http = {
  request<T>(config: HttpRequestConfig): Promise<HttpResponse<T>> {
    return request<T>(config)
  },
  get<T>(url: string, options: HttpRequestOptions = {}): Promise<HttpResponse<T>> {
    return request<T>({ ...options, method: "GET", url })
  },
  post<T>(url: string, data?: unknown, options: HttpRequestOptions = {}): Promise<HttpResponse<T>> {
    return request<T>({ ...options, method: "POST", url, data })
  },
  put<T>(url: string, data?: unknown, options: HttpRequestOptions = {}): Promise<HttpResponse<T>> {
    return request<T>({ ...options, method: "PUT", url, data })
  },
  patch<T>(url: string, data?: unknown, options: HttpRequestOptions = {}): Promise<HttpResponse<T>> {
    return request<T>({ ...options, method: "PATCH", url, data })
  },
  delete<T>(url: string, options: HttpRequestOptions = {}): Promise<HttpResponse<T>> {
    return request<T>({ ...options, method: "DELETE", url })
  },
}

export const httpData = {
  request<T>(config: HttpRequestConfig): Promise<T> {
    return requestData<T>(config)
  },
  get<T>(url: string, options: HttpRequestOptions = {}): Promise<T> {
    return requestData<T>({ ...options, method: "GET", url })
  },
  post<T>(url: string, data?: unknown, options: HttpRequestOptions = {}): Promise<T> {
    return requestData<T>({ ...options, method: "POST", url, data })
  },
  put<T>(url: string, data?: unknown, options: HttpRequestOptions = {}): Promise<T> {
    return requestData<T>({ ...options, method: "PUT", url, data })
  },
  patch<T>(url: string, data?: unknown, options: HttpRequestOptions = {}): Promise<T> {
    return requestData<T>({ ...options, method: "PATCH", url, data })
  },
  delete<T>(url: string, options: HttpRequestOptions = {}): Promise<T> {
    return requestData<T>({ ...options, method: "DELETE", url })
  },
}

export function normalizeHttpError(error: unknown, fallback = "Request failed"): HttpRequestError {
  return toHttpRequestError(error, fallback)
}

export async function tryHttp<T>(operation: Promise<T> | (() => Promise<T>)): Promise<HttpResult<T>> {
  try {
    const data = await (typeof operation === "function" ? operation() : operation)
    return { ok: true, data }
  } catch (error) {
    const normalized = normalizeHttpError(error)
    return {
      ok: false,
      error: normalized,
      message: normalized.message,
      status: normalized.status,
    }
  }
}
