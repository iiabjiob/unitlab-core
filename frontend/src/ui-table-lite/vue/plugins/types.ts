// src/ui-table/plugins/types.ts
// Shared plugin interfaces for UiTable.

export type UiTablePluginEventHandler = (...args: any[]) => void

export interface UiTablePluginSetupContext {
  /** Unique identifier of the table instance installing the plugin. */
  tableId: string
  /** Returns the nearest theme/root element for DOM-based integrations. */
  getRootElement: () => HTMLElement | null
  /** Provides access to the exposed public API of the table component. */
  getHostExpose: () => Record<string, unknown>
  /** Emits a host (component-level) event, mirroring `emit` + config handlers. */
  emitHostEvent: (event: string, ...args: any[]) => void
  /** Subscribes to plugin/local events. Returns an unsubscribe function. */
  on: (event: string, handler: UiTablePluginEventHandler) => () => void
  /** Emits a plugin/local event to other subscribers. */
  emit: (event: string, ...args: any[]) => void
  /** Registers a cleanup hook that runs when the plugin is disposed. */
  registerCleanup: (cleanup: () => void) => void
}

export interface UiTablePlugin {
  id: string
  setup(context: UiTablePluginSetupContext): void | (() => void)
}

export type UiTablePluginDefinition = UiTablePlugin | (() => UiTablePlugin)
