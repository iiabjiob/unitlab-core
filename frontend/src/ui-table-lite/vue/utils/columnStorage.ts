import { inject } from "vue"
import { UiTableColumnStorageKey, type ColumnVisibilitySnapshot } from "../context"

export interface ColumnStorageAccessor {
  load: () => ColumnVisibilitySnapshot[] | null
  save: (snapshot: ColumnVisibilitySnapshot[]) => void
  clear: () => void
}

function getLocalStorage(): Storage | null {
  if (typeof window === "undefined") {
    return null
  }
  try {
    return window.localStorage ?? null
  } catch {
    return null
  }
}

function sanitizeLoadedSnapshot(value: unknown): ColumnVisibilitySnapshot[] | null {
  if (!Array.isArray(value)) {
    return null
  }
  const sanitized: ColumnVisibilitySnapshot[] = []
  for (const entry of value) {
    if (!entry || typeof entry !== "object") {
      continue
    }
    const record = entry as Record<string, unknown>
    const key = record.key
    if (typeof key !== "string") {
      continue
    }
    const visible = record.visible !== false
    const label = typeof record.label === "string" ? record.label : undefined
    sanitized.push({ key, label, visible })
  }
  return sanitized
}

function normalizeSnapshot(snapshot: ColumnVisibilitySnapshot[]): ColumnVisibilitySnapshot[] {
  const normalized: ColumnVisibilitySnapshot[] = []
  snapshot.forEach(entry => {
    if (!entry || typeof entry.key !== "string") {
      return
    }
    normalized.push({
      key: entry.key,
      label: typeof entry.label === "string" ? entry.label : undefined,
      visible: entry.visible !== false,
    })
  })
  return normalized
}

function readFromLocalStorage(storageKey: string): ColumnVisibilitySnapshot[] | null {
  const storage = getLocalStorage()
  if (!storage) {
    return null
  }
  try {
    const raw = storage.getItem(storageKey)
    if (!raw) {
      return null
    }
    const parsed = JSON.parse(raw) as unknown
    return sanitizeLoadedSnapshot(parsed)
  } catch (error) {
    console.warn("UiTable: failed to load column visibility state from localStorage", error)
    return null
  }
}

function writeToLocalStorage(storageKey: string, snapshot: ColumnVisibilitySnapshot[]): void {
  const storage = getLocalStorage()
  if (!storage) {
    return
  }
  try {
    storage.setItem(storageKey, JSON.stringify(snapshot))
  } catch (error) {
    console.warn("UiTable: failed to persist column visibility state to localStorage", error)
  }
}

function clearLocalStorage(storageKey: string): void {
  const storage = getLocalStorage()
  if (!storage) {
    return
  }
  try {
    storage.removeItem(storageKey)
  } catch (error) {
    console.warn("UiTable: failed to clear column visibility state from localStorage", error)
  }
}

export function createColumnStorageAccessor(storageKey: string): ColumnStorageAccessor {
  const adapter = inject(UiTableColumnStorageKey, null)

  function load(): ColumnVisibilitySnapshot[] | null {
    if (adapter) {
      try {
        const stored = adapter.get(storageKey)
        const sanitized = sanitizeLoadedSnapshot(stored as unknown)
        if (sanitized) {
          return sanitized
        }
        if (Array.isArray(stored) && stored.length === 0) {
          return []
        }
      } catch (error) {
        console.warn("UiTable: failed to load column visibility state from adapter", error)
      }
    }
    return readFromLocalStorage(storageKey)
  }

  function save(snapshot: ColumnVisibilitySnapshot[]): void {
    const normalized = normalizeSnapshot(snapshot)
    if (adapter) {
      try {
        adapter.set(storageKey, normalized)
        return
      } catch (error) {
        console.warn("UiTable: failed to persist column visibility state via adapter", error)
      }
    }
    writeToLocalStorage(storageKey, normalized)
  }

  function clear(): void {
    if (adapter?.remove) {
      try {
        adapter.remove(storageKey)
        return
      } catch (error) {
        console.warn("UiTable: failed to clear column visibility state via adapter", error)
      }
    }
    clearLocalStorage(storageKey)
  }

  return { load, save, clear }
}

export type { ColumnVisibilitySnapshot } from "../context"
