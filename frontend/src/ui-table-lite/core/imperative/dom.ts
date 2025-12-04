import type { StyleCache } from "./types.js"
import type { ClassLike } from "./bindings.js"

export function mergeClasses(...classes: ClassLike[]): string {
  const result: string[] = []
  for (const value of classes) {
    if (!value) continue
    if (typeof value === "string") {
      if (value) result.push(value)
      continue
    }
    if (Array.isArray(value)) {
      result.push(mergeClasses(...value))
      continue
    }
    for (const [key, active] of Object.entries(value)) {
      if (active) result.push(key)
    }
  }
  return result.join(" ").trim()
}

export function toDisplayValue(value: unknown): string {
  if (value == null) return ""
  if (typeof value === "number" && !Number.isFinite(value)) return ""
  if (value instanceof Date) return value.toLocaleString()
  if (typeof value === "object") {
    try {
      return JSON.stringify(value)
    } catch (error) {
      console.warn("[ImperativeTableBody] Failed to stringify cell value", error)
      return String(value)
    }
  }
  return String(value)
}

export function setVisibility(element: HTMLElement, visible: boolean) {
  element.style.visibility = visible ? "" : "hidden"
  element.style.pointerEvents = visible ? "" : "none"
}

export function applyStyle(element: HTMLElement, cache: StyleCache, next: Record<string, unknown>) {
  const seen = new Set<string>()
  for (const [key, value] of Object.entries(next)) {
    seen.add(key)
    const previous = cache.get(key)
    if (previous === value) continue
    cache.set(key, value)
    if (value == null || value === "") {
      if (key.includes("-")) {
        element.style.removeProperty(key)
      } else {
        ;(element.style as any)[key] = ""
      }
      continue
    }
    if (key.includes("-")) {
      element.style.setProperty(key, String(value))
    } else {
      ;(element.style as any)[key] = value as any
    }
  }
  for (const key of Array.from(cache.keys())) {
    if (seen.has(key)) continue
    cache.delete(key)
    if (key.includes("-")) {
      element.style.removeProperty(key)
    } else {
      ;(element.style as any)[key] = ""
    }
  }
}
