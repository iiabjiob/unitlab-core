import { computed, shallowRef, type ComputedRef, type Ref } from "vue"
import {
  createServerRowModel,
  type ServerRowModelDiagnostics,
  type ServerRowModelOptions as CoreServerRowModelOptions,
  type ServerRowModelDebug,
} from "../../core/serverRowModel/serverRowModel"

export type { ServerRowModelFetchResult } from "../../core/serverRowModel/serverRowModel"
export type { ServerRowModelDiagnostics } from "../../core/serverRowModel/serverRowModel"

export interface UseServerRowModelOptions<T> extends CoreServerRowModelOptions<T> {}

export interface ServerRowModel<T> {
  rows: ComputedRef<T[]>
  loading: ComputedRef<boolean>
  error: Ref<Error | null>
  blocks: ComputedRef<Map<number, T[]>>
  total: Ref<number | null>
  loadedRanges: ComputedRef<Array<{ start: number; end: number }>>
  progress: ComputedRef<number | null>
  blockErrors: ComputedRef<Map<number, Error>>
  getRowAt: (index: number) => T | undefined
  getRowCount: () => number
  refreshBlock: (blockIndex: number) => Promise<void>
  fetchBlock: (startIndex: number) => Promise<void>
  diagnostics: ComputedRef<ServerRowModelDiagnostics>
  __debug?: ServerRowModelDebug<T>
  reset: () => void
  abortAll: () => void
  dispose: () => void
}

export function useServerRowModel<T>(options: UseServerRowModelOptions<T>): ServerRowModel<T> {
  const createSignal = <S>(initial: S) => shallowRef(initial)
  const model = createServerRowModel<T>(options, { createSignal })

  const rows = computed(() => model.rows.value)
  const loading = computed(() => model.loading.value)
  const blocks = computed(() => model.blocks.value)
  const loadedRanges = computed(() => model.loadedRanges.value)
  const progress = computed(() => model.progress.value)
  const blockErrors = computed(() => model.blockErrors.value)
  const diagnostics = computed(() => model.diagnostics.value)
  const error = computed({
    get: () => model.error.value,
    set: next => {
      model.error.value = next
    },
  })
  const total = computed({
    get: () => model.total.value,
    set: next => {
      model.total.value = next
    },
  })

  return {
    rows,
    loading,
    error,
    blocks,
    total,
    loadedRanges,
    progress,
    blockErrors,
    getRowAt: model.getRowAt,
    getRowCount: model.getRowCount,
    refreshBlock: model.refreshBlock,
    fetchBlock: model.fetchBlock,
    diagnostics,
    __debug: model.__debug,
    reset: model.reset,
    abortAll: model.abortAll,
    dispose: model.dispose,
  }
}
