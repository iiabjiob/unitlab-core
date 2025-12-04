// src/ui-table/theme/colorUtils.ts
// Helpers for converting palette colors into consistent CSS color strings.

function normalizeHex(hex: string): string {
  const trimmed = hex.trim()
  if (!trimmed) throw new Error("Color value cannot be empty")
  const value = trimmed.startsWith("#") ? trimmed.slice(1) : trimmed
  if (![3, 6].includes(value.length)) {
    throw new Error(`Unsupported hex color: ${hex}`)
  }
  if (!/^([0-9a-fA-F]+)$/.test(value)) {
    throw new Error(`Invalid hex color: ${hex}`)
  }
  if (value.length === 3) {
    return `#${value[0]}${value[0]}${value[1]}${value[1]}${value[2]}${value[2]}`.toLowerCase()
  }
  return `#${value.toLowerCase()}`
}

interface RgbComponents {
  r: number
  g: number
  b: number
}

function hexToRgbComponents(hex: string): RgbComponents {
  const normalized = normalizeHex(hex)
  const value = normalized.slice(1)
  const r = parseInt(value.slice(0, 2), 16)
  const g = parseInt(value.slice(2, 4), 16)
  const b = parseInt(value.slice(4, 6), 16)
  return { r, g, b }
}

function formatAlpha(alpha?: number): string | undefined {
  if (alpha == null) return undefined
  const clamped = Math.min(Math.max(alpha, 0), 1)
  if (clamped === 1) return undefined
  // Keep up to 3 decimal places without trailing zeros for readability.
  const formatted = Number.parseFloat(clamped.toFixed(3))
  return formatted.toString()
}

function formatRgb({ r, g, b }: RgbComponents, alpha?: number): string {
  const alphaPart = formatAlpha(alpha)
  if (alphaPart) {
    return `rgb(${r} ${g} ${b} / ${alphaPart})`
  }
  return `rgb(${r} ${g} ${b})`
}

export function toRgb(hex: string): string {
  return formatRgb(hexToRgbComponents(hex))
}

export function toRgba(hex: string, alpha: number): string {
  return formatRgb(hexToRgbComponents(hex), alpha)
}

export function transparent(): string {
  return toRgba("#000000", 0)
}

export { normalizeHex }
