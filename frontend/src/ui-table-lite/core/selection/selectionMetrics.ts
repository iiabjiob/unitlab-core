import type {
  UiTableSelectionMetricContext,
  UiTableSelectionMetricDefinition,
  UiTableSelectionMetricResult,
  UiTableSelectionMetricsConfig,
  UiTableSelectedCell,
} from "../types"

export type SelectionMetricComputer = (context: UiTableSelectionMetricContext) => number | null

export const BUILTIN_SELECTION_METRIC_COMPUTERS: Record<string, SelectionMetricComputer> = {
  count: context => context.cellCount,
  sum: context => context.sum,
  min: context => (context.numericCount > 0 ? context.min : null),
  max: context => (context.numericCount > 0 ? context.max : null),
  avg: context => context.average,
  average: context => context.average,
}

export type NumberFormatterResolver = (precision?: number) => Intl.NumberFormat

export interface CreateFormatterResolverOptions {
  baseFormatter?: Intl.NumberFormat
}

export function createNumberFormatterResolver(options?: CreateFormatterResolverOptions): NumberFormatterResolver {
  const cache = new Map<number, Intl.NumberFormat>()
  const base = options?.baseFormatter ?? new Intl.NumberFormat()

  return (precision?: number) => {
    if (precision === undefined) {
      return base
    }
    if (!cache.has(precision)) {
      cache.set(
        precision,
        new Intl.NumberFormat(undefined, {
          minimumFractionDigits: precision,
          maximumFractionDigits: precision,
        }),
      )
    }
    return cache.get(precision) ?? base
  }
}

export interface ComputeSelectionMetricsOptions {
  config: UiTableSelectionMetricsConfig
  cells: UiTableSelectedCell[]
  labelMap: Record<string, string>
  getNumberFormatter: NumberFormatterResolver
  computers?: Record<string, SelectionMetricComputer>
  minCellCount?: number
}

function coerceNumericValue(raw: unknown): number | null {
  if (typeof raw === "number") {
    return Number.isFinite(raw) ? raw : null
  }
  if (typeof raw === "bigint") {
    return Number(raw)
  }
  if (typeof raw === "boolean") {
    return raw ? 1 : 0
  }
  if (typeof raw === "string") {
    const normalized = raw.replace(/\s+/g, "").replace(/,/g, "")
    const parsed = Number.parseFloat(normalized)
    return Number.isFinite(parsed) ? parsed : null
  }
  if (raw != null) {
    const coerced = Number(raw)
    return Number.isFinite(coerced) ? coerced : null
  }
  return null
}

function formatSelectionMetricValue(
  definition: UiTableSelectionMetricDefinition,
  label: string,
  rawValue: number | null | undefined,
  context: UiTableSelectionMetricContext,
  resolveFormatter: NumberFormatterResolver,
): string {
  const value = rawValue ?? null

  if (definition.formatter) {
    try {
      const formatted = definition.formatter({
        id: definition.id,
        label,
        value,
        context,
      })
      if (typeof formatted === "string") {
        return formatted
      }
      if (formatted !== undefined && formatted !== null) {
        return String(formatted)
      }
    } catch (error) {
      if (typeof console !== "undefined" && typeof console.warn === "function") {
        console.warn(`UiTable selection metric formatter failed for "${definition.id}":`, error)
      }
    }
  }

  if (value === null || Number.isNaN(value)) {
    return "—"
  }

  if (!Number.isFinite(value)) {
    return String(value)
  }

  const formatter = resolveFormatter(definition.precision)
  return formatter.format(value)
}

export function computeSelectionMetrics(options: ComputeSelectionMetricsOptions): UiTableSelectionMetricResult[] {
  const { config, cells, labelMap, getNumberFormatter, computers = BUILTIN_SELECTION_METRIC_COMPUTERS, minCellCount = 2 } = options

  if (!config.enabled || config.metrics.length === 0) {
    return []
  }

  if (cells.length < minCellCount) {
    return []
  }

  const numericValues: number[] = []

  for (const cell of cells) {
    const numeric = coerceNumericValue(cell.value)
    if (numeric !== null && Number.isFinite(numeric)) {
      numericValues.push(numeric)
    }
  }

  const numericCount = numericValues.length
  let sum: number | null = null
  let min: number | null = null
  let max: number | null = null
  let average: number | null = null

  if (numericCount > 0) {
    sum = numericValues.reduce((total, value) => total + value, 0)
    min = Math.min(...numericValues)
    max = Math.max(...numericValues)
    average = sum / numericCount
  }

  const context: UiTableSelectionMetricContext = {
    cells,
    cellCount: cells.length,
    numericValues,
    numericCount,
    sum,
    min,
    max,
    average,
  }

  const definitions = config.metrics.filter(definition => {
    if (definition.id === "count") {
      return true
    }
    if (typeof definition.compute === "function") {
      return true
    }
    return numericCount > 0
  })

  const results: UiTableSelectionMetricResult[] = []

  for (const definition of definitions) {
    const label = definition.label ?? labelMap[definition.id] ?? definition.id
    const fallbackComputer = computers[definition.id]

    let rawValue: number | null | undefined
    try {
      if (typeof definition.compute === "function") {
        rawValue = definition.compute(context)
      } else if (fallbackComputer) {
        rawValue = fallbackComputer(context)
      } else {
        rawValue = null
      }
    } catch (error) {
      rawValue = null
      if (typeof console !== "undefined" && typeof console.warn === "function") {
        console.warn(`UiTable selection metric compute failed for "${definition.id}":`, error)
      }
    }

    if (definition.id === "count" && (rawValue === undefined || rawValue === null)) {
      rawValue = context.cellCount
    }

    if (definition.id !== "count" && (rawValue === undefined || rawValue === null)) {
      continue
    }

    let value: number | null
    if (rawValue === null) {
      value = null
    } else if (typeof rawValue === "number") {
      value = rawValue
    } else {
      const coerced = Number(rawValue)
      value = Number.isFinite(coerced) ? coerced : null
    }

    if (definition.id !== "count" && (value === null || Number.isNaN(value))) {
      continue
    }

    const displayValue = formatSelectionMetricValue(definition, label, value, context, getNumberFormatter)

    results.push({
      id: definition.id,
      label,
      value,
      displayValue,
    })
  }

  return results
}
