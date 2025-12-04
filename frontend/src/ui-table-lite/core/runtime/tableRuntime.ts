import type { UiTableEventHandlers } from "../types"
import type { UiTablePluginDefinition } from "../../../ui-table/plugins/types"

type PluginManagerOptions = {
  emitHostEvent: (event: string, ...args: unknown[]) => void
  getTableId?: () => string
  getRootElement?: () => HTMLElement | null
  getHostExpose?: () => Record<string, unknown>
}

class NoopPluginManager {
  constructor(_options: PluginManagerOptions) {}

  setPlugins(_plugins: UiTablePluginDefinition[]): void {}

  notify(_eventName: string, ..._args: unknown[]): void {}

  destroy(): void {}
}

export const HOST_EVENT_NAME_MAP = {
  reachBottom: "reach-bottom",
  rowClick: "row-click",
  cellEdit: "cell-edit",
  batchEdit: "batch-edit",
  selectionChange: "selection-change",
  sortChange: "sort-change",
  filterChange: "filter-change",
  filtersReset: "filters-reset",
  zoomChange: "zoom-change",
  columnResize: "column-resize",
  groupFilterToggle: "group-filter-toggle",
  rowsDelete: "rows-delete",
  lazyLoad: "lazy-load",
  lazyLoadComplete: "lazy-load-complete",
  lazyLoadError: "lazy-load-error",
  autoResizeApplied: "auto-resize-applied",
  selectAllRequest: "select-all-request",
} as const

export type HostEventName = keyof typeof HOST_EVENT_NAME_MAP

export type EventArgs<K extends HostEventName> = NonNullable<UiTableEventHandlers[K]> extends (
  ...args: infer P
) => any
  ? P
  : []

export function isHostEventName(value: string): value is HostEventName {
  return value in HOST_EVENT_NAME_MAP
}

interface TableRuntimeOptions {
  onHostEvent: (name: HostEventName, args: readonly unknown[]) => void
  onUnknownPluginEvent?: (event: string, args: readonly unknown[]) => void
  pluginContext: {
    getTableId: () => string
    getRootElement: () => HTMLElement | null
    getHostExpose: () => Record<string, unknown>
  }
  initialHandlers?: UiTableEventHandlers | undefined
  initialPlugins?: UiTablePluginDefinition[] | undefined
}

export interface TableRuntime {
  emit<K extends HostEventName>(name: K, ...args: EventArgs<K>): void
  setHostHandlers(handlers: UiTableEventHandlers | undefined): void
  setPlugins(plugins: UiTablePluginDefinition[] | undefined): void
  dispose(): void
}

class InternalTableRuntime implements TableRuntime {
  private handlers: UiTableEventHandlers
  private readonly onHostEvent: (name: HostEventName, args: readonly unknown[]) => void
  private readonly onUnknownPluginEvent?: (event: string, args: readonly unknown[]) => void
  private readonly pluginManager: NoopPluginManager

  constructor(options: TableRuntimeOptions) {
    this.handlers = options.initialHandlers ?? {}
    this.onHostEvent = options.onHostEvent
    this.onUnknownPluginEvent = options.onUnknownPluginEvent

    this.pluginManager = new NoopPluginManager({
      getTableId: options.pluginContext.getTableId,
      getRootElement: options.pluginContext.getRootElement,
      getHostExpose: options.pluginContext.getHostExpose,
      emitHostEvent: (event, ...args) => this.handlePluginHostEvent(event, args),
    })

    if (options.initialPlugins) {
      this.setPlugins(options.initialPlugins)
    }
  }

  emit<K extends HostEventName>(name: K, ...args: EventArgs<K>): void {
    this.invokeHandlers(name, args)
  }

  setHostHandlers(handlers: UiTableEventHandlers | undefined): void {
    this.handlers = handlers ?? {}
  }

  setPlugins(plugins: UiTablePluginDefinition[] | undefined): void {
    this.pluginManager.setPlugins(Array.isArray(plugins) ? plugins : [])
  }

  dispose(): void {
    this.pluginManager.destroy()
  }

  private handlePluginHostEvent(event: string, args: unknown[]): void {
    if (isHostEventName(event)) {
      this.invokeHandlers(event, args)
      return
    }
    this.onUnknownPluginEvent?.(event, args)
  }

  private invokeHandlers(name: HostEventName, args: readonly unknown[]): void {
    const handler = this.handlers?.[name]
    if (typeof handler === "function") {
      ;(handler as (...params: readonly unknown[]) => void)(...args)
    }
    this.pluginManager.notify(String(name), ...(args as unknown[]))
    this.onHostEvent(name, args)
  }
}

export function createTableRuntime(options: TableRuntimeOptions): TableRuntime {
  return new InternalTableRuntime(options)
}
