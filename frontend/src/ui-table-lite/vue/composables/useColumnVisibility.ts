import { computed, nextTick, ref, type Ref } from "vue"
import type { UiTableColumn } from "../../core/types"
import { createColumnStorageAccessor, type ColumnVisibilitySnapshot } from "../utils/columnStorage"

export type VisibilitySnapshot = ColumnVisibilitySnapshot

interface AutoColumnResizeAdapter {
  invalidate: () => void
  syncManualState: (columns: UiTableColumn[]) => void
}

interface UseColumnVisibilityOptions {
  tableId: Ref<string | number | undefined>
  localColumns: Ref<UiTableColumn[]>
  autoColumnResize: AutoColumnResizeAdapter
  scheduleAutoColumnResize: () => void
  reorderPinnedColumns: () => void
  updateViewportHeight?: () => void
  scheduleOverlayUpdate?: () => void
}

const VISIBILITY_STORAGE_PREFIX = "uiTable.columns.visibility"

export function useColumnVisibility(options: UseColumnVisibilityOptions) {
  const visibilityStorageKey = computed(() => `${VISIBILITY_STORAGE_PREFIX}.${options.tableId.value}`)
  const columnVisibilityState = ref<Record<string, boolean>>({})
  const visibilityHydrated = ref(false)

  function buildColumnSnapshot(columns: UiTableColumn[]): VisibilitySnapshot[] {
    return columns.map(column => ({ key: column.key, label: column.label, visible: column.visible !== false }))
  }

  function updateVisibilityMapFromColumns(columns: UiTableColumn[]) {
    const snapshot = buildColumnSnapshot(columns)
    const next: Record<string, boolean> = {}
    snapshot.forEach(entry => {
      next[entry.key] = entry.visible
    })
    columnVisibilityState.value = next
    return snapshot
  }

  function persistColumnState(snapshot?: VisibilitySnapshot[]) {
    const storage = createColumnStorageAccessor(visibilityStorageKey.value)
    const payload = snapshot ?? buildColumnSnapshot(options.localColumns.value)
    storage.save(payload)
  }

  function loadColumnStateFromStorage(): VisibilitySnapshot[] | null {
    const storage = createColumnStorageAccessor(visibilityStorageKey.value)
    return storage.load()
  }

  function applyStoredColumnState(snapshot: VisibilitySnapshot[]) {
    if (!snapshot.length) {
      updateVisibilityMapFromColumns(options.localColumns.value)
      return
    }

    const orderMap = new Map<string, { index: number; visible: boolean }>()
    snapshot.forEach((entry, index) => {
      orderMap.set(entry.key, { index, visible: entry.visible !== false })
    })

    const sortedColumns = [...options.localColumns.value]
      .map((column, originalIndex) => ({ column: { ...column }, originalIndex }))
      .sort((a, b) => {
        const orderA = orderMap.has(a.column.key)
          ? orderMap.get(a.column.key)!.index
          : a.column.isSystem
            ? -1
            : Number.MAX_SAFE_INTEGER
        const orderB = orderMap.has(b.column.key)
          ? orderMap.get(b.column.key)!.index
          : b.column.isSystem
            ? -1
            : Number.MAX_SAFE_INTEGER
        if (orderA !== orderB) {
          return orderA - orderB
        }
        return a.originalIndex - b.originalIndex
      })
      .map(entry => {
        const stored = orderMap.get(entry.column.key)
        return stored ? { ...entry.column, visible: stored.visible } : entry.column
      })

    options.localColumns.value = sortedColumns
    const snapshotToPersist = updateVisibilityMapFromColumns(sortedColumns)
    persistColumnState(snapshotToPersist)
    options.autoColumnResize.invalidate()
    options.autoColumnResize.syncManualState(options.localColumns.value)
    options.scheduleAutoColumnResize()
  }

  function handleColumnsVisibilityUpdate(payload: VisibilitySnapshot[]) {
    if (!payload.length) return
    applyStoredColumnState(payload)
    nextTick(() => {
      options.updateViewportHeight?.()
      options.scheduleOverlayUpdate?.()
    })
  }

  function resetColumnVisibility() {
    options.localColumns.value = options.localColumns.value.map(column => {
      return { ...column, visible: true }
    })
    const snapshot = updateVisibilityMapFromColumns(options.localColumns.value)
    persistColumnState(snapshot)

    nextTick(() => {
      options.updateViewportHeight?.()
      options.scheduleOverlayUpdate?.()
    })
    options.autoColumnResize.invalidate()
    options.autoColumnResize.syncManualState(options.localColumns.value)
    options.scheduleAutoColumnResize()
  }

  function hideColumn(columnKey: string) {
    const index = options.localColumns.value.findIndex(column => column.key === columnKey)
    if (index === -1) {
      return
    }
    const target = options.localColumns.value[index]
    if (target.isSystem) {
      return
    }
    const nextColumns = options.localColumns.value.map(column =>
      column.key === columnKey ? { ...column, visible: false } : column,
    )
    options.localColumns.value = nextColumns
    const snapshot = updateVisibilityMapFromColumns(nextColumns)
    persistColumnState(snapshot)
    options.reorderPinnedColumns()
    nextTick(() => {
      options.updateViewportHeight?.()
      options.scheduleOverlayUpdate?.()
    })
    options.autoColumnResize.invalidate()
    options.autoColumnResize.syncManualState(options.localColumns.value)
    options.scheduleAutoColumnResize()
  }

  return {
    visibilityStorageKey,
    columnVisibilityState,
    visibilityHydrated,
    updateVisibilityMapFromColumns,
    persistColumnState,
    loadColumnStateFromStorage,
    applyStoredColumnState,
    handleColumnsVisibilityUpdate,
    resetColumnVisibility,
    hideColumn,
  }
}
