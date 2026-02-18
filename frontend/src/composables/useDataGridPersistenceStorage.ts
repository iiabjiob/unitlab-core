type WidthMap = Record<string, number>

function resolveDatasetStorageKey(tableId: string): string {
  return `affino-datagrid-dataset::${tableId}`
}

function resolveColumnWidthsStorageKey(tableId: string, datasetKey: string): string {
  return `affino-datagrid-widths::${tableId}::${datasetKey}`
}

function resolveSelectionStorageKey(tableId: string, datasetKey: string): string {
  return `affino-datagrid-selection::${tableId}::${datasetKey}`
}

function resolveGlobalSelectionStorageKey(tableId: string): string {
  return `affino-datagrid-selection::${tableId}::__global__`
}

export function readPersistedColumnWidths(tableId: string, datasetKey: string): WidthMap | null {
  if (typeof window === "undefined") {
    return null
  }
  try {
    const raw = window.localStorage.getItem(resolveColumnWidthsStorageKey(tableId, datasetKey))
    if (!raw) return null
    const parsed = JSON.parse(raw) as Record<string, unknown>
    if (!parsed || typeof parsed !== "object") {
      return null
    }
    const normalized: WidthMap = {}
    Object.entries(parsed).forEach(([key, value]) => {
      const width = Number(value)
      if (!Number.isFinite(width) || width <= 0) return
      normalized[key] = Math.max(1, Math.trunc(width))
    })
    return Object.keys(normalized).length ? normalized : null
  } catch {
    return null
  }
}

export function writePersistedColumnWidths(tableId: string, datasetKey: string, widths: WidthMap) {
  if (typeof window === "undefined") {
    return
  }
  try {
    window.localStorage.setItem(
      resolveColumnWidthsStorageKey(tableId, datasetKey),
      JSON.stringify(widths),
    )
  } catch {
    // Ignore storage write failures and keep runtime functional.
  }
}

export function readPersistedSelection(tableId: string, datasetKey: string): Set<string> | null {
  if (typeof window === "undefined") {
    return null
  }
  try {
    const keys = [
      resolveSelectionStorageKey(tableId, datasetKey),
      resolveGlobalSelectionStorageKey(tableId),
    ]
    for (const key of keys) {
      const raw = window.localStorage.getItem(key)
      if (!raw) {
        continue
      }
      const parsed = JSON.parse(raw)
      if (!Array.isArray(parsed)) {
        continue
      }
      return new Set(
        parsed
          .map(item => String(item ?? "").trim())
          .filter(item => item.length > 0),
      )
    }
    return null
  } catch {
    return null
  }
}

export function writePersistedSelection(tableId: string, datasetKey: string, rowKeys: readonly string[]) {
  if (typeof window === "undefined") {
    return
  }
  try {
    const payload = JSON.stringify([...rowKeys])
    window.localStorage.setItem(resolveSelectionStorageKey(tableId, datasetKey), payload)
    window.localStorage.setItem(resolveGlobalSelectionStorageKey(tableId), payload)
  } catch {
    // Ignore storage write failures and keep runtime functional.
  }
}

export function readPersistedDatasetKey(tableId: string): string | null {
  if (typeof window === "undefined") {
    return null
  }
  try {
    return window.localStorage.getItem(resolveDatasetStorageKey(tableId))
  } catch {
    return null
  }
}

export function writePersistedDatasetKey(tableId: string, datasetKey: string) {
  if (typeof window === "undefined") {
    return
  }
  try {
    window.localStorage.setItem(resolveDatasetStorageKey(tableId), datasetKey)
  } catch {
    // Ignore storage write failures and keep runtime functional.
  }
}
