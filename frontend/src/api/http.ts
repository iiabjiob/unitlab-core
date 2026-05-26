import { toUserFacingErrorMessage } from "@/api/errorMessages"
import {
  HttpRequestError,
  type HttpMethod,
  normalizeHttpDetail,
} from "@/api/httpErrors"

type HttpResponseType = "json" | "text" | "blob" | "arraybuffer"

export type HttpRequestOptions = {
  headers?: Record<string, string>
  params?: Record<string, unknown>
  data?: unknown
  timeout?: number
  signal?: AbortSignal
  responseType?: HttpResponseType
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

const DEFAULT_TIMEOUT_MS = 30000
const JSON_CONTENT_TYPE = "application/json"
const RETRY_DELAY_MS = 160
const MAX_READ_RETRIES = 3

type FetchAttemptConfig = HttpRequestConfig & {
  retryCount: number
}

function isFormData(value: unknown): value is FormData {
  return typeof FormData !== "undefined" && value instanceof FormData
}

function isBlobBody(value: unknown): value is Blob {
  return typeof Blob !== "undefined" && value instanceof Blob
}

function isUrlSearchParamsBody(value: unknown): value is URLSearchParams {
  return typeof URLSearchParams !== "undefined" && value instanceof URLSearchParams
}

function isBodyInit(value: unknown): value is BodyInit {
  return (
    typeof value === "string"
    || value instanceof ArrayBuffer
    || ArrayBuffer.isView(value)
    || isBlobBody(value)
    || isFormData(value)
    || isUrlSearchParamsBody(value)
  )
}

function appendQueryParams(url: string, params?: Record<string, unknown>): string {
  if (!params) return url

  const query = new URLSearchParams()
  Object.entries(params).forEach(([key, value]) => {
    if (value === undefined || value === null) return
    if (Array.isArray(value)) {
      value.forEach((item) => {
        if (item !== undefined && item !== null) {
          query.append(key, String(item))
        }
      })
      return
    }
    query.set(key, String(value))
  })

  const queryString = query.toString()
  if (!queryString) return url
  return `${url}${url.includes("?") ? "&" : "?"}${queryString}`
}

function normalizeRequestHeaders(headers?: Record<string, string>, body?: unknown): Headers {
  const normalized = new Headers(headers ?? {})
  if (isFormData(body)) {
    normalized.delete("Content-Type")
    return normalized
  }
  if (body !== undefined && !isBodyInit(body) && !normalized.has("Content-Type")) {
    normalized.set("Content-Type", JSON_CONTENT_TYPE)
  }
  return normalized
}

function createRequestBody(data: unknown): BodyInit | undefined {
  if (data === undefined || data === null) return undefined
  if (isBodyInit(data)) return data
  return JSON.stringify(data)
}

function mergeSignals(
  timeoutMs: number,
  externalSignal?: AbortSignal,
): { signal: AbortSignal; cleanup: () => void; timedOut: () => boolean } {
  const controller = new AbortController()
  let didTimeout = false
  const timeoutId = globalThis.setTimeout(() => {
    didTimeout = true
    controller.abort()
  }, timeoutMs)

  const abortFromExternal = () => controller.abort()
  if (externalSignal) {
    if (externalSignal.aborted) {
      controller.abort()
    } else {
      externalSignal.addEventListener("abort", abortFromExternal, { once: true })
    }
  }

  return {
    signal: controller.signal,
    cleanup: () => {
      globalThis.clearTimeout(timeoutId)
      externalSignal?.removeEventListener("abort", abortFromExternal)
    },
    timedOut: () => didTimeout,
  }
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return Boolean(value) && typeof value === "object" && !Array.isArray(value)
}

function responseHeadersToRecord(headers: Headers): Record<string, unknown> {
  const record: Record<string, unknown> = {}
  headers.forEach((value, key) => {
    record[key] = value
  })
  return record
}

async function readResponseData(response: Response, responseType?: HttpResponseType): Promise<unknown> {
  if (response.status === 204 || response.status === 205) {
    return null
  }
  if (responseType === "text") {
    return response.text()
  }
  if (responseType === "blob") {
    return response.blob()
  }
  if (responseType === "arraybuffer") {
    return response.arrayBuffer()
  }

  const contentType = response.headers.get("content-type") ?? ""
  if (responseType === "json" || contentType.includes("application/json")) {
    const text = await response.text()
    return text ? JSON.parse(text) : null
  }
  return response.text()
}

function createHttpRequestError(
  message: string,
  context: {
    status?: number | null
    detail?: string
    url: string
    method: string
    code?: string
    responseData?: unknown
  },
  cause?: unknown,
): HttpRequestError {
  return new HttpRequestError(message, {
    status: context.status ?? null,
    detail: context.detail ?? "",
    url: context.url,
    method: context.method.toUpperCase(),
    code: context.code ?? "",
    message,
    responseData: context.responseData ?? null,
    isHttpError: true,
  }, { cause })
}

function isRetryableTransportError(error: unknown, config: HttpRequestConfig): boolean {
  const method = String(config.method ?? "GET").toLowerCase()
  if (method !== "get") return false

  const url = String(config.url ?? "")
  const isApiReadEndpoint = /\/api\/v\d+\//.test(url) || /^\/api\//.test(url)
  const code = error instanceof HttpRequestError ? error.code : ""
  const message = error instanceof Error ? String(error.message ?? "") : ""
  const contentLengthMismatch = /content_length_mismatch|content-length|length\s*mismatch/i.test(message)
  const networkLike = code === "ERR_NETWORK" || code === "ECONNRESET" || code === "ETIMEDOUT"

  return isApiReadEndpoint && (contentLengthMismatch || networkLike)
}

function toHttpRequestError(error: unknown, fallback = "Request failed"): HttpRequestError {
  if (error instanceof HttpRequestError) {
    return error
  }
  return createHttpRequestError(
    toUserFacingErrorMessage(error, fallback),
    { url: "", method: "" },
    error,
  )
}

async function fetchAttempt<T>(config: FetchAttemptConfig): Promise<HttpResponse<T>> {
  const params = config.retryCount > 0
    ? { ...(config.params ?? {}), __retryTs: Date.now() }
    : config.params
  const url = appendQueryParams(config.url, params)
  const body = createRequestBody(config.data)
  const timeoutMs = Math.max(1, Number(config.timeout ?? DEFAULT_TIMEOUT_MS))
  const timeoutState = mergeSignals(timeoutMs, config.signal)

  try {
    const response = await fetch(url, {
      method: config.method,
      headers: normalizeRequestHeaders(config.headers, config.data),
      body,
      signal: timeoutState.signal,
      cache: config.method === "GET" ? "no-store" : undefined,
    })
    const responseData = await readResponseData(response, config.responseType)
    const responseUrl = response.url || url
    if (!response.ok) {
      const detail = normalizeHttpDetail(isRecord(responseData) ? responseData.detail : "")
      const rawError = createHttpRequestError(response.statusText || "Request failed", {
        status: response.status,
        detail,
        url: responseUrl,
        method: config.method,
        responseData,
      })
      throw createHttpRequestError(toUserFacingErrorMessage(rawError), {
        status: response.status,
        detail,
        url: responseUrl,
        method: config.method,
        responseData,
      }, rawError)
    }
    return {
      data: responseData as T,
      status: response.status,
      statusText: response.statusText,
      headers: responseHeadersToRecord(response.headers),
      url: responseUrl,
      method: config.method,
    }
  } catch (error) {
    if (error instanceof HttpRequestError) {
      throw error
    }
    const code = timeoutState.timedOut() ? "ECONNABORTED" : "ERR_NETWORK"
    const message = timeoutState.timedOut()
      ? `Request timed out after ${timeoutMs} ms`
      : (error instanceof Error && error.message ? error.message : "Network request failed")
    throw createHttpRequestError(message, {
      url,
      method: config.method,
      code,
    }, error)
  } finally {
    timeoutState.cleanup()
  }
}

async function request<T>(config: HttpRequestConfig): Promise<HttpResponse<T>> {
  let retryCount = 0
  while (true) {
    try {
      return await fetchAttempt<T>({ ...config, retryCount })
    } catch (error) {
      if (retryCount >= MAX_READ_RETRIES || !isRetryableTransportError(error, config)) {
        throw error
      }
      const delayMs = RETRY_DELAY_MS * (retryCount + 1)
      retryCount += 1
      await new Promise(resolve => globalThis.setTimeout(resolve, delayMs))
    }
  }
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
