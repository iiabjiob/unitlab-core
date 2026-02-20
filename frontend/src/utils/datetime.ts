/**
 * Format timestamp to HH:mm:ss.SSS (compact time only).
 */
export function formatTs(ms: number): string {
  const d = new Date(ms)
  const hh = String(d.getHours()).padStart(2, "0")
  const mm = String(d.getMinutes()).padStart(2, "0")
  const ss = String(d.getSeconds()).padStart(2, "0")
  const ms3 = String(d.getMilliseconds()).padStart(3, "0")
  return `${hh}:${mm}:${ss}.${ms3}`
}

/**
 * Format timestamp to YYYY-MM-DD HH:mm:ss.SSS (full date + time).
 */
export function formatTsFull(ms: number): string {
  const d = new Date(ms)
  const yyyy = d.getFullYear()
  const MM = String(d.getMonth() + 1).padStart(2, "0")
  const dd = String(d.getDate()).padStart(2, "0")
  const hh = String(d.getHours()).padStart(2, "0")
  const mm = String(d.getMinutes()).padStart(2, "0")
  const ss = String(d.getSeconds()).padStart(2, "0")
  const ms3 = String(d.getMilliseconds()).padStart(3, "0")
  return `${yyyy}-${MM}-${dd} ${hh}:${mm}:${ss}.${ms3}`
}

/**
 * Convenience wrapper to format ISO strings or Date objects using formatTsFull.
 */
export function formatDate(value: string | number | Date): string {
  const ms = typeof value === "number" ? value : new Date(value).getTime()
  return formatTsFull(ms)
}

export function formatDateShort(value: string | number | Date, locale = "en-GB"): string {
  const date = new Date(value)
  if (Number.isNaN(date.getTime())) {
    return "--"
  }
  return date.toLocaleDateString(locale, {
    year: "numeric",
    month: "short",
    day: "2-digit",
  })
}
