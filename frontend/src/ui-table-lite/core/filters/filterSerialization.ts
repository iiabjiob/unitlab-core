const NULL_FILTER_KEY = "__NULL__"
const UNDEFINED_FILTER_KEY = "__UNDEFINED__"

export function serializeFilterValue(value: unknown): string {
  if (value === null) return NULL_FILTER_KEY
  if (value === undefined) return UNDEFINED_FILTER_KEY
  if (value instanceof Date) return value.toISOString()
  if (typeof value === "object") {
    try {
      return JSON.stringify(value)
    } catch {
      return String(value)
    }
  }
  return String(value)
}

export function formatFilterLabel(value: unknown): string {
  if (value === null || value === undefined) return "(empty)"
  if (typeof value === "string" && value.trim() === "") return "(blank)"
  if (typeof value === "boolean") return value ? "True" : "False"
  return String(value)
}

export function deserializeFilterValue(value: unknown): unknown {
  if (typeof value !== "string") return value

  if (value === NULL_FILTER_KEY || value === UNDEFINED_FILTER_KEY) {
    return null
  }

  const trimmed = value.trim()
  if (trimmed === "true") return true
  if (trimmed === "false") return false

  const isLikelyJson =
    (trimmed.startsWith("{") && trimmed.endsWith("}")) ||
    (trimmed.startsWith("[") && trimmed.endsWith("]"))

  if (isLikelyJson) {
    try {
      return JSON.parse(trimmed)
    } catch {
      return value
    }
  }

  return value
}
