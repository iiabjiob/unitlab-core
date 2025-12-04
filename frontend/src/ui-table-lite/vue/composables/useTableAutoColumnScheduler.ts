import { nextTick, ref, watch, type ComputedRef, type Ref } from "vue"
import type { UiTableColumn } from "../../core/types"

interface AutoColumnResizeAdapter {
  applyAutoColumnWidths(args: {
    columns: UiTableColumn[]
    viewportWidth: number
    zoom: number
  }): Map<string, number> | null | undefined
  syncManualState(columns: UiTableColumn[]): void
}

interface UseTableAutoColumnSchedulerOptions {
  autoColumnResize: AutoColumnResizeAdapter
  visibilityHydrated: Ref<boolean>
  localColumns: Ref<UiTableColumn[]>
  viewportWidth: Ref<number>
  zoomLayoutScale: ComputedRef<number>
  refreshViewport: () => void
  scheduleOverlayUpdate: () => void
}

interface UseTableAutoColumnSchedulerResult {
  scheduleAutoColumnResize: () => void
  enableAutoColumnResize: () => void
}

export function useTableAutoColumnScheduler(
  options: UseTableAutoColumnSchedulerOptions,
): UseTableAutoColumnSchedulerResult {
  const autoResizeReady = ref(false)
  const pendingAutoColumnResize = ref(false)

  const runAutoColumnResize = async () => {
    if (!autoResizeReady.value) return
    if (!options.visibilityHydrated.value) return

    const width = options.viewportWidth.value
    if (!width || width <= 0) return

    const columnsSnapshot = options.localColumns.value
    if (!Array.isArray(columnsSnapshot) || columnsSnapshot.length === 0) {
      return
    }

    const updates = options.autoColumnResize.applyAutoColumnWidths({
      columns: columnsSnapshot,
      viewportWidth: width,
      zoom: options.zoomLayoutScale.value,
    })

    if (!updates || updates.size === 0) {
      return
    }

    const nextColumns = columnsSnapshot.map(column =>
      updates.has(column.key) ? { ...column, width: updates.get(column.key)! } : column,
    )

    options.localColumns.value = nextColumns
    options.autoColumnResize.syncManualState(nextColumns)

    await nextTick()
    options.refreshViewport()
    options.scheduleOverlayUpdate()
  }

  const scheduleAutoColumnResize = () => {
    if (!autoResizeReady.value) return
    if (pendingAutoColumnResize.value) return
    pendingAutoColumnResize.value = true
    nextTick()
      .then(runAutoColumnResize)
      .finally(() => {
        pendingAutoColumnResize.value = false
      })
  }

  const enableAutoColumnResize = () => {
    if (autoResizeReady.value) return
    autoResizeReady.value = true
    scheduleAutoColumnResize()
  }

  watch(
    () => options.viewportWidth.value,
    () => {
      if (!autoResizeReady.value) return
      scheduleAutoColumnResize()
    },
  )

  return {
    scheduleAutoColumnResize,
    enableAutoColumnResize,
  }
}
