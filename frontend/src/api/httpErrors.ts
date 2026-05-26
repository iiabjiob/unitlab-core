export type HttpMethod = "GET" | "POST" | "PUT" | "PATCH" | "DELETE"

export type HttpErrorContext = {
  status: number | null
  detail: string
  url: string
  method: string
  code: string
  message: string
  responseData: unknown
  isHttpError: boolean
}

export class HttpRequestError extends Error {
  readonly name = "HttpRequestError"
  readonly status: number | null
  readonly detail: string
  readonly url: string
  readonly method: string
  readonly code: string
  readonly responseData: unknown

  constructor(message: string, context: HttpErrorContext, options?: { cause?: unknown }) {
    super(message)
    if (options && "cause" in options) {
      (this as Error & { cause?: unknown }).cause = options.cause
    }
    this.status = context.status
    this.detail = context.detail
    this.url = context.url
    this.method = context.method
    this.code = context.code
    this.responseData = context.responseData
  }
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return Boolean(value) && typeof value === "object" && !Array.isArray(value)
}

export function normalizeHttpDetail(detail: unknown): string {
  if (typeof detail === "string") {
    return detail.trim()
  }
  if (Array.isArray(detail)) {
    return detail
      .map((entry) => {
        if (typeof entry === "string") return entry
        if (isRecord(entry) && typeof entry.msg === "string") return entry.msg
        return ""
      })
      .filter(Boolean)
      .join("; ")
      .trim()
  }
  if (isRecord(detail)) {
    const message = detail.msg ?? detail.message
    if (typeof message === "string") {
      return message.trim()
    }
  }
  return ""
}

function readHttpStatus(value: unknown): number | null {
  const status = Number(value)
  return Number.isFinite(status) ? status : null
}

function readRecordPath(source: Record<string, unknown>, path: string[]): unknown {
  let cursor: unknown = source
  for (const key of path) {
    if (!isRecord(cursor)) return undefined
    cursor = cursor[key]
  }
  return cursor
}

export function getHttpErrorContext(error: unknown): HttpErrorContext {
  if (error instanceof HttpRequestError) {
    return {
      status: error.status,
      detail: error.detail,
      url: error.url,
      method: error.method,
      code: error.code,
      message: error.message,
      responseData: error.responseData,
      isHttpError: true,
    }
  }

  const record = isRecord(error) ? error : {}
  const responseData = readRecordPath(record, ["response", "data"]) ?? record.data
  const detail = normalizeHttpDetail(
    isRecord(responseData) ? responseData.detail : record.detail,
  )
  const status = readHttpStatus(readRecordPath(record, ["response", "status"]) ?? record.status)
  const url = String(readRecordPath(record, ["config", "url"]) ?? record.url ?? "")
  const method = String(readRecordPath(record, ["config", "method"]) ?? record.method ?? "").toUpperCase()
  const code = String(record.code ?? "")
  const message = error instanceof Error ? error.message : String(record.message ?? "")
  const isHttpError = (
    status !== null
    || Boolean(detail)
    || Boolean(url)
    || Boolean(code)
    || Boolean(responseData)
  )

  return {
    status,
    detail,
    url,
    method,
    code,
    message,
    responseData,
    isHttpError,
  }
}

export function isHttpRequestError(error: unknown): error is HttpRequestError {
  return error instanceof HttpRequestError
}
