import axios from "axios"

type ErrorContext = {
  status: number | null
  detail: string
  url: string
}

function normalizeDetail(detail: unknown): string {
  if (typeof detail === "string") {
    return detail.trim()
  }
  if (Array.isArray(detail)) {
    const joined = detail
      .map((entry) => {
        if (typeof entry === "string") {
          return entry
        }
        if (entry && typeof entry === "object") {
          const message = (entry as { msg?: unknown }).msg
          if (typeof message === "string") {
            return message
          }
        }
        return ""
      })
      .filter(Boolean)
      .join("; ")
    return joined.trim()
  }
  if (detail && typeof detail === "object") {
    const message = (detail as { msg?: unknown; message?: unknown }).msg ?? (detail as { message?: unknown }).message
    if (typeof message === "string") {
      return message.trim()
    }
  }
  return ""
}

function extractErrorContext(error: unknown): ErrorContext {
  if (!axios.isAxiosError(error)) {
    return { status: null, detail: "", url: "" }
  }
  const status = Number(error.response?.status)
  const detail = normalizeDetail(error.response?.data?.detail)
  const url = String(error.config?.url ?? "")
  return {
    status: Number.isFinite(status) ? status : null,
    detail,
    url,
  }
}

function mapKnownDetail(detail: string): string | null {
  const tooManySignalsMatch = detail.match(/Too many signals for test run \(max\s*(\d+)\)/i)
  if (tooManySignalsMatch) {
    const max = tooManySignalsMatch[1]
    return `Too many signals selected for test run. Maximum is ${max}. Reduce selection or run in batches.`
  }

  return null
}

function mapByStatus(context: ErrorContext): string | null {
  const { status, url } = context
  if (status === 409 && /\/signal-allocations\/test-run\/jobs(?:\?|$)/.test(url)) {
    return "A test run is already active in this workspace. Stop it or wait until it finishes."
  }
  if (status === 401) {
    return "Your session is not authorized. Please sign in and try again."
  }
  if (status === 403) {
    return "You do not have permission to perform this action."
  }
  if (status === 404) {
    return "Requested resource was not found. Refresh the page and try again."
  }
  if (status === 422) {
    return "Some input values are invalid. Check the form and try again."
  }
  if (status !== null && status >= 500) {
    return "Server error occurred. Please try again in a moment."
  }
  return null
}

export function toUserFacingErrorMessage(error: unknown, fallback = "Request failed"): string {
  if (!axios.isAxiosError(error)) {
    if (error instanceof Error && String(error.message || "").trim()) {
      return error.message
    }
    return fallback
  }

  const context = extractErrorContext(error)

  const mappedDetail = context.detail ? mapKnownDetail(context.detail) : null
  if (mappedDetail) {
    return mappedDetail
  }

  const mappedByStatus = mapByStatus(context)
  if (mappedByStatus) {
    return mappedByStatus
  }

  if (context.detail) {
    return context.detail
  }

  const message = String(error.message ?? "").trim()
  if (message && !/request failed with status code/i.test(message)) {
    return message
  }

  return fallback
}
