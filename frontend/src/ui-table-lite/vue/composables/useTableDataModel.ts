import { computed, onBeforeUnmount, onMounted, ref, shallowRef, watch, type ComputedRef } from "vue"
import type { NormalizedTableProps } from "../../core/config/tableConfig"
import type {
  UiTableLazyLoadContext,
  UiTableLazyLoadReason,
  UiTableServerLoadResult,
  UiTableSortState,
  UiTableFilterSnapshot,
} from "../../core/types"
import { useServerRowModel } from "./useServerRowModel"
import type { EventArgs, HostEventName } from "../../core/runtime/tableRuntime"

const SERVER_PLACEHOLDER_FLAG = "__uiTableServerPlaceholder__"

type EmitHostEvent = <K extends HostEventName>(name: K, ...args: EventArgs<K>) => void

type ServerSortResolver = () => UiTableSortState[]
type ServerFilterResolver = () => UiTableFilterSnapshot | null

interface UseTableDataModelOptions {
  normalizedProps: ComputedRef<NormalizedTableProps>
  emit: (event: string, ...args: any[]) => void
  fireEvent: EmitHostEvent
  getServerSorts: ServerSortResolver
  getServerFilters: ServerFilterResolver
}

export function useTableDataModel(options: UseTableDataModelOptions) {
  const internalLazyLoading = ref(false)
  const propRows = computed(() => options.normalizedProps.value.rows)
  const resolvedPageSize = computed(() => options.normalizedProps.value.pageSize)
  const isLazyMode = computed(() => typeof options.normalizedProps.value.lazyLoader === "function")
  const serverSideModel = computed(() => isLazyMode.value && options.normalizedProps.value.serverSideModel)
  const serverSideFiltering = computed(() => serverSideModel.value)
  const serverSideSorting = computed(() => serverSideModel.value)
  const autoLazyOnScroll = computed(() => isLazyMode.value && options.normalizedProps.value.autoLoadOnScroll)
  const autoLazyOnMount = computed(() => isLazyMode.value && options.normalizedProps.value.loadOnMount)

  const lazyPendingPage = ref<number | null>(null)
  const lastCompletedPage = ref(-1)
  const lastKnownRowLength = ref(0)
  const lazyErrorState = ref<unknown | null>(null)
  const serverRowLoadReason = ref<UiTableLazyLoadReason>("manual")
  const scrollToTopForServer = shallowRef<(() => void) | null>(null)

  const serverRowModel = useServerRowModel<any>({
    blockSize: resolvedPageSize.value,
    loadBlock: async ({ start, limit, signal, background }) => {
      const loader = options.normalizedProps.value.lazyLoader
      if (!loader) {
        return { rows: [] }
      }

      const blockSize = limit || resolvedPageSize.value
      const page = Math.floor(start / Math.max(1, blockSize))
      const reason = background ? "scroll" : serverRowLoadReason.value
      const sorts = serverSideSorting.value ? options.getServerSorts() : undefined
      const filtersSnapshot = serverSideFiltering.value ? options.getServerFilters() : null

      const eventContext: UiTableLazyLoadContext = {
        page,
        pageSize: blockSize,
        offset: start,
        totalLoaded: serverRowLoadedCount.value,
        reason,
        sorts,
        filters: filtersSnapshot,
        mode: "block",
      }

      if (!background) {
        lazyPendingPage.value = page
        internalLazyLoading.value = true
        lazyErrorState.value = null
        options.emit("lazy-load", eventContext)
        options.fireEvent("lazyLoad", eventContext)
      }

      try {
        const response = await loader({
          ...eventContext,
          signal,
          background,
          blockSize,
        })
        if (signal.aborted) {
          return { rows: [] }
        }

        let rows: any[] = []
        let total: number | undefined

        if (Array.isArray(response)) {
          rows = response
        } else if (response && typeof response === "object") {
          const payload = response as UiTableServerLoadResult<any>
          if (Array.isArray(payload.rows)) {
            rows = payload.rows
          }
          if (typeof payload.total === "number") {
            total = payload.total
          }
        }

        if (!background) {
          const completion = { ...eventContext }
          options.emit("lazy-load-complete", completion)
          options.fireEvent("lazyLoadComplete", completion)
        }

        return total != null ? { rows, total } : rows
      } catch (error) {
        if (!background) {
          lazyErrorState.value = error
          const payload = { ...eventContext, error }
          options.emit("lazy-load-error", payload)
          options.fireEvent("lazyLoadError", payload)
        }
        throw error
      } finally {
        if (!background) {
          if (lazyPendingPage.value === page) {
            lazyPendingPage.value = null
          }
          internalLazyLoading.value = false
          serverRowLoadReason.value = "scroll"
        }
      }
    },
  })

  const serverRowBlocks = computed(() => serverRowModel.blocks.value)
  const serverRowLoadedCount = computed(() => {
    let total = 0
    serverRowBlocks.value.forEach(block => {
      total += block.length
    })
    return total
  })

  const placeholderRowCache = new Map<number, Record<string, unknown>>()

  function getPlaceholderRow(index: number): Record<string, unknown> {
    const cached = placeholderRowCache.get(index)
    if (cached) {
      return cached
    }
    const placeholder: Record<string, unknown> = {
      [SERVER_PLACEHOLDER_FLAG]: true,
      __rowIndex: index,
    }
    placeholderRowCache.set(index, placeholder)
    return placeholder
  }

  function clearPlaceholderRows() {
    placeholderRowCache.clear()
  }

  const serverRowTotal = computed(() => {
    const explicit = options.normalizedProps.value.totalRows
    if (typeof explicit === "number" && Number.isFinite(explicit)) {
      return Math.max(0, Math.floor(explicit))
    }
    const modelTotal = serverRowModel.total.value
    if (typeof modelTotal === "number" && Number.isFinite(modelTotal)) {
      return Math.max(0, Math.floor(modelTotal))
    }
    let max = 0
    serverRowBlocks.value.forEach((rows, start) => {
      if (rows.length) {
        max = Math.max(max, start + rows.length)
      }
    })
    return max
  })

  const serverRows = computed(() => {
    if (!serverSideModel.value) {
      return [] as any[]
    }
    const total = serverRowTotal.value
    if (!Number.isFinite(total) || total <= 0) {
      const partial: any[] = []
      serverRowBlocks.value.forEach(blockRows => {
        if (blockRows.length) {
          for (let index = 0; index < blockRows.length; index += 1) {
            const row = blockRows[index]
            if (row != null) {
              partial.push(row)
            }
          }
        }
      })
      return partial
    }
    const buffer = new Array<any>(total)
    serverRowBlocks.value.forEach((blockRows, start) => {
      for (let index = 0; index < blockRows.length; index += 1) {
        const row = blockRows[index]
        if (row != null) {
          const absolute = start + index
          buffer[absolute] = row
          placeholderRowCache.delete(absolute)
        }
      }
    })
    for (let index = 0; index < total; index += 1) {
      if (buffer[index] === undefined) {
        buffer[index] = getPlaceholderRow(index)
      }
    }
    return buffer
  })

  const resolvedRows = computed(() => (serverSideModel.value ? serverRows.value : propRows.value))
  const loadingState = computed(() => options.normalizedProps.value.loading || internalLazyLoading.value)

  const computedHasMore = computed(() => {
    if (serverSideModel.value) {
      return false
    }
    const explicit = options.normalizedProps.value.hasMore
    if (explicit != null) {
      return explicit
    }
    const totalRows = options.normalizedProps.value.totalRows
    if (totalRows != null) {
      return resolvedRows.value.length < totalRows
    }
    return true
  })

  watch(
    () => resolvedRows.value.length,
    (length, previousLength) => {
      lastKnownRowLength.value = length
      const derivedPage = Math.max(-1, Math.ceil(length / Math.max(1, resolvedPageSize.value)) - 1)
      lastCompletedPage.value = derivedPage
      if (lazyPendingPage.value != null && derivedPage >= lazyPendingPage.value) {
        lazyPendingPage.value = null
        internalLazyLoading.value = false
      }
      if (previousLength !== undefined && length < (previousLength ?? 0)) {
        lazyErrorState.value = null
      }
    },
    { immediate: true },
  )

  watch(
    () => options.normalizedProps.value.loading,
    value => {
      if (!value && lazyPendingPage.value == null) {
        internalLazyLoading.value = false
      }
    },
  )

  watch(
    () => serverSideModel.value,
    value => {
      if (value) {
        clearPlaceholderRows()
        serverRowModel.reset()
        serverRowLoadReason.value = autoLazyOnMount.value ? "mount" : "manual"
        void serverRowModel.fetchBlock(0)
        return
      }
      clearPlaceholderRows()
      serverRowModel.reset()
    },
    { immediate: true },
  )

  onMounted(() => {
    if (serverSideModel.value) {
      return
    }
    if (autoLazyOnMount.value && computedHasMore.value && resolvedRows.value.length === 0) {
      void requestNextPage("mount")
    }
  })

  onBeforeUnmount(() => {
    if (serverSideModel.value) {
      serverRowModel.abortAll()
    }
    clearPlaceholderRows()
  })

  function buildLazyLoadContext(page: number, reason: UiTableLazyLoadReason): UiTableLazyLoadContext {
    const context: UiTableLazyLoadContext = {
      page,
      pageSize: resolvedPageSize.value,
      offset: page * resolvedPageSize.value,
      totalLoaded: resolvedRows.value.length,
      reason,
    }
    if (serverSideSorting.value) {
      context.sorts = options.getServerSorts()
    }
    if (serverSideFiltering.value) {
      context.filters = options.getServerFilters()
    }
    return context
  }

  async function requestPage(page: number, reason: UiTableLazyLoadReason = "manual") {
    if (!isLazyMode.value) {
      if (!loadingState.value && page > lastCompletedPage.value) {
        options.emit("reach-bottom")
        options.fireEvent("reachBottom")
      }
      return
    }

    if (page < 0) {
      return
    }

    if (page !== 0 && !computedHasMore.value) {
      return
    }

    if (internalLazyLoading.value || lazyPendingPage.value != null) {
      return
    }

    const context = buildLazyLoadContext(page, reason)

    options.emit("lazy-load", context)
    options.fireEvent("lazyLoad", context)

    const loader = options.normalizedProps.value.lazyLoader
    if (!loader) {
      return
    }

    lazyPendingPage.value = page
    internalLazyLoading.value = true
    lazyErrorState.value = null

    try {
      await loader(context)
      lastCompletedPage.value = Math.max(lastCompletedPage.value, page)
      const completionPayload = { ...context }
      options.emit("lazy-load-complete", completionPayload)
      options.fireEvent("lazyLoadComplete", completionPayload)
    } catch (error) {
      lazyErrorState.value = error
      const errorPayload = { ...context, error }
      options.emit("lazy-load-error", errorPayload)
      options.fireEvent("lazyLoadError", errorPayload)
      lazyPendingPage.value = null
      internalLazyLoading.value = false
      throw error
    } finally {
      if (lazyPendingPage.value != null && lazyPendingPage.value <= lastCompletedPage.value) {
        lazyPendingPage.value = null
      }
      if (lazyPendingPage.value == null) {
        internalLazyLoading.value = false
      }
    }
  }

  async function requestNextPage(reason: UiTableLazyLoadReason = "manual") {
    const nextPage = Math.max(-1, lastCompletedPage.value) + 1
    await requestPage(nextPage, reason)
  }

  function resetLazyPaging(options?: { clearRows?: boolean }) {
    lazyPendingPage.value = null
    internalLazyLoading.value = false
    lazyErrorState.value = null
    if (options?.clearRows) {
      lastCompletedPage.value = -1
      lastKnownRowLength.value = 0
      return
    }
    const length = resolvedRows.value.length
    lastKnownRowLength.value = length
    lastCompletedPage.value = Math.max(-1, Math.ceil(length / Math.max(1, resolvedPageSize.value)) - 1)
  }

  const lastServerFilterFingerprint = ref<string | null>(null)
  const lastServerSortFingerprint = ref<string | null>(null)
  let scheduledServerReason: UiTableLazyLoadReason | null = null
  let flushServerReloadTask: Promise<void> | null = null

  async function reloadServerRowModel(reason: UiTableLazyLoadReason) {
    clearPlaceholderRows()
    serverRowModel.reset()
    serverRowLoadReason.value = reason
    scrollToTopForServer.value?.()
    await serverRowModel.fetchBlock(0)
  }

  function flushServerReload() {
    if (!scheduledServerReason) return
    if (internalLazyLoading.value || lazyPendingPage.value != null) {
      return
    }
    const reason = scheduledServerReason
    scheduledServerReason = null
    flushServerReloadTask = null
    if (serverSideModel.value) {
      void reloadServerRowModel(reason)
      return
    }
    void triggerServerReload(reason)
  }

  function scheduleServerReload(reason: UiTableLazyLoadReason) {
    if (!serverSideModel.value) return
    const prioritized = reason === "filter-change"
    if (!scheduledServerReason || prioritized) {
      scheduledServerReason = reason
    }
    if (internalLazyLoading.value || lazyPendingPage.value != null) {
      return
    }
    if (flushServerReloadTask) {
      return
    }
    flushServerReloadTask = Promise.resolve().then(() => flushServerReload())
  }

  async function triggerServerReload(reason: UiTableLazyLoadReason) {
    if (serverSideModel.value) {
      await reloadServerRowModel(reason)
      return
    }
    if (!isLazyMode.value) return
    resetLazyPaging({ clearRows: true })
    await requestPage(0, reason)
  }

  watch(
    () => [internalLazyLoading.value, lazyPendingPage.value],
    () => {
      if (!internalLazyLoading.value && lazyPendingPage.value == null) {
        flushServerReload()
      }
    },
  )

  function isServerPlaceholderRow(row: unknown): row is Record<string, unknown> {
    return Boolean(row && typeof row === "object" && SERVER_PLACEHOLDER_FLAG in (row as Record<string, unknown>))
  }

  function setScrollToTopForServer(fn: (() => void) | null) {
    scrollToTopForServer.value = fn
  }

  return {
    internalLazyLoading,
    isLazyMode,
    serverSideModel,
    serverSideFiltering,
    serverSideSorting,
    autoLazyOnScroll,
    autoLazyOnMount,
    resolvedRows,
    loadingState,
    computedHasMore,
    requestPage,
    requestNextPage,
    resetLazyPaging,
    triggerServerReload,
    scheduleServerReload,
    serverRowModel,
    serverRowBlocks,
    serverRowTotal,
    serverRows,
    clearPlaceholderRows,
    isServerPlaceholderRow,
    lazyPendingPage,
    lazyErrorState,
    lastServerFilterFingerprint,
    lastServerSortFingerprint,
    setScrollToTopForServer,
  }
}
