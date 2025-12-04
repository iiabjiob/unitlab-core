import type { UiTableColumn } from "../types/column"
import {
  deserializeFilterValue as baseDeserializeFilterValue,
  formatFilterLabel as baseFormatFilterLabel,
  serializeFilterValue as baseSerializeFilterValue,
} from "../filters/filterSerialization"

export function toNumberValue(value: unknown): number | null {
  if (value === null || value === undefined || value === "") return null
  if (typeof value === "number") {
    return Number.isFinite(value) ? value : null
  }
  const numeric = Number(value)
  return Number.isFinite(numeric) ? numeric : null
}

export function numericCompare(a: unknown, b: unknown): number {
  const numA = toNumberValue(a)
  const numB = toNumberValue(b)
  if (numA === null && numB === null) return 0
  if (numA === null) return 1
  if (numB === null) return -1
  return numA - numB
}

export function toComparableString(value: unknown): string | null {
  if (value === null || value === undefined) return null
  return String(value)
}

export function stringCompare(a: unknown, b: unknown): number {
  const strA = toComparableString(a)
  const strB = toComparableString(b)
  if (strA === null && strB === null) return 0
  if (strA === null) return 1
  if (strB === null) return -1
  return strA.localeCompare(strB, undefined, { sensitivity: "base" })
}

export function createComparator(column?: UiTableColumn | null) {
  if (column?.filterType === "number" || column?.editor === "number") return numericCompare
  if (column?.filterType === "date") return dateCompare
  return stringCompare
}

export function toDateValue(value: unknown): number | null {
  if (value === null || value === undefined || value === "") return null
  if (value instanceof Date) return Number.isFinite(value.getTime()) ? value.getTime() : null
  const parsed = Date.parse(String(value))
  return Number.isFinite(parsed) ? parsed : null
}

export function dateCompare(a: unknown, b: unknown): number {
  const timeA = toDateValue(a)
  const timeB = toDateValue(b)
  if (timeA === null && timeB === null) return 0
  if (timeA === null) return 1
  if (timeB === null) return -1
  return timeA - timeB
}

export function serializeFilterValue(value: unknown): string {
  return baseSerializeFilterValue(value)
}

export function formatFilterLabel(value: unknown): string {
  return baseFormatFilterLabel(value)
}

export function deserializeFilterValue(value: unknown): unknown {
  return baseDeserializeFilterValue(value)
}
