import { getHttpErrorContext, type HttpErrorContext } from "@/api/httpErrors"

function mapKnownDetail(detail: string): string | null {
  const tooManySignalsMatch = detail.match(/Too many signals for test run \(max\s*(\d+)\)/i)
  if (tooManySignalsMatch) {
    const max = tooManySignalsMatch[1]
    return `Too many signals selected for test run. Maximum is ${max}. Reduce selection or run in batches.`
  }

  return null
}

function mapByStatus(context: HttpErrorContext): string | null {
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
  const context = getHttpErrorContext(error)

  if (!context.isHttpError) {
    if (error instanceof Error && String(error.message || "").trim()) {
      return error.message
    }
    return fallback
  }

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

  const message = String(context.message ?? "").trim()
  if (message && !/request failed with status code/i.test(message)) {
    return message
  }

  return fallback
}
