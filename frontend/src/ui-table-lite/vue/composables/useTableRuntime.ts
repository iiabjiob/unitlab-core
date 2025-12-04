import { onBeforeUnmount, shallowRef, watch, type ComputedRef, type Ref } from "vue"
import {
  HOST_EVENT_NAME_MAP,
  createTableRuntime,
  type EventArgs,
  type HostEventName,
  type TableRuntime,
} from "../../core/runtime/tableRuntime"
import type { NormalizedTableProps } from "../../core/config/tableConfig"
import type { UiTableExposeBindings } from "../context"

type HostEventEmitName = (typeof HOST_EVENT_NAME_MAP)[HostEventName]

interface UseTableRuntimeOptions {
  normalizedProps: ComputedRef<NormalizedTableProps>
  emit: (event: HostEventEmitName, ...args: any[]) => void
  tableId: ComputedRef<string>
  tableRootRef: Ref<HTMLElement | null>
}

interface UseTableRuntimeResult {
  fireEvent: <K extends HostEventName>(name: K, ...args: EventArgs<K>) => void
  setTableExpose: (expose: UiTableExposeBindings) => void
}

export function useTableRuntime(options: UseTableRuntimeOptions): UseTableRuntimeResult {
  const tableExposeRef = shallowRef<UiTableExposeBindings | null>(null)
  const runtime = shallowRef<TableRuntime | null>(
    createTableRuntime({
      onHostEvent: (event, args) => {
        options.emit(HOST_EVENT_NAME_MAP[event], ...args)
      },
      onUnknownPluginEvent: (event, args) => {
        if (typeof console !== "undefined" && console.warn) {
          console.warn(`[UiTable] Plugin attempted to emit unknown host event "${event}"`, args)
        }
      },
      pluginContext: {
        getTableId: () => options.tableId.value,
        getRootElement: () =>
          options.tableRootRef.value?.closest?.(".ui-table-theme-root") ?? options.tableRootRef.value,
        getHostExpose: () => (tableExposeRef.value ?? {}) as Record<string, unknown>,
      },
      initialHandlers: options.normalizedProps.value.events,
      initialPlugins: options.normalizedProps.value.plugins,
    })
  )

  const fireEvent = <K extends HostEventName>(name: K, ...args: EventArgs<K>) => {
    runtime.value?.emit(name, ...args)
  }

  watch(
    () => options.normalizedProps.value.events,
    handlers => {
      runtime.value?.setHostHandlers(handlers)
    },
    { deep: true }
  )

  watch(
    () => options.normalizedProps.value.plugins,
    plugins => {
      runtime.value?.setPlugins(plugins)
    },
    { deep: true }
  )

  onBeforeUnmount(() => {
    runtime.value?.dispose()
    runtime.value = null
    tableExposeRef.value = null
  })

  const setTableExpose = (expose: UiTableExposeBindings) => {
    tableExposeRef.value = expose
  }

  return {
    fireEvent,
    setTableExpose,
  }
}
