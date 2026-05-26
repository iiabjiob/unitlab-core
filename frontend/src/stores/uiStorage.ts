import { defineStore } from "pinia"
import { ref } from "vue"

import { localSettingsKeys, readLocalSetting, writeLocalSetting } from "@/services/localSettingsStorage"

// Panel identifiers whose sizes we persist between sessions.
export type PanelSizeMap = {
  leftAside: number
  pageSidebar: number
  deviceChannels: number
  switchgearList: number
  sequenceSteps: number
}

type UiPreferences = {
  panelSizes?: Partial<PanelSizeMap>
  collapsed?: Partial<Record<"leftAside" | "pageSidebar", boolean>>
}

export const useUiStore = defineStore("uiStore", () => {
  // --- STATE --------------------------------------------------------

  // Keep every resizable panel width inside one object for easier persistence/extension.
  const panelSizes = ref<PanelSizeMap>({
    leftAside: 280,
    pageSidebar: 300,
    deviceChannels: 600,
    switchgearList: 620,
    sequenceSteps: 720,
  })

  // Collapsed flags for major chrome elements.
  const collapsed = ref({
    leftAside: false,
    pageSidebar: false,
  })

  // --- ACTIONS -------------------------------------------------------

  function setPanelSize(panel: keyof PanelSizeMap, value: number) {
    panelSizes.value[panel] = value
    persist() // persist manually instead of using Pinia plugins
  }

  function toggleCollapse(panel: keyof typeof collapsed.value) {
    collapsed.value[panel] = !collapsed.value[panel]
    persist()
  }

  // --- PERSISTENCE ---------------------------------------------------

  const LEGACY_STORAGE_KEY = "unitlab.ui"

  // Serialize current UI preferences through the local settings adapter.
  function persist() {
    writeLocalSetting(localSettingsKeys.uiChrome, {
      panelSizes: panelSizes.value,
      collapsed: collapsed.value,
    }, {
      legacyKeys: [LEGACY_STORAGE_KEY],
    })
  }

  // Restore previously saved values, falling back to defaults if parsing fails.
  function restore() {
    const parsed = readLocalSetting<UiPreferences | null>(localSettingsKeys.uiChrome, null, {
      legacyKeys: [LEGACY_STORAGE_KEY],
      validate: normalizeUiPreferences,
    })
    if (!parsed) return

    if (parsed.panelSizes) {
      panelSizes.value = {
        ...panelSizes.value,
        ...parsed.panelSizes,
      }
    }

    if (parsed.collapsed) {
      collapsed.value = {
        ...collapsed.value,
        ...parsed.collapsed,
      }
    }
  }

  return {
    // state
    panelSizes,
    collapsed,

    // actions
    setPanelSize,
    toggleCollapse,
    persist,
    restore,
  }
})

function normalizeUiPreferences(value: unknown): UiPreferences | null {
  if (!isRecord(value)) {
    return null
  }

  const panelSizes = isRecord(value.panelSizes)
    ? normalizePanelSizes(value.panelSizes)
    : undefined
  const collapsed = isRecord(value.collapsed)
    ? normalizeCollapsedState(value.collapsed)
    : undefined

  return { panelSizes, collapsed }
}

function normalizePanelSizes(value: Record<string, unknown>): Partial<PanelSizeMap> {
  const next: Partial<PanelSizeMap> = {}
  const keys: Array<keyof PanelSizeMap> = [
    "leftAside",
    "pageSidebar",
    "deviceChannels",
    "switchgearList",
    "sequenceSteps",
  ]

  keys.forEach((key) => {
    const size = Number(value[key])
    if (Number.isFinite(size) && size > 0) {
      next[key] = size
    }
  })

  return next
}

function normalizeCollapsedState(
  value: Record<string, unknown>,
): Partial<Record<"leftAside" | "pageSidebar", boolean>> {
  const next: Partial<Record<"leftAside" | "pageSidebar", boolean>> = {}
  if (typeof value.leftAside === "boolean") {
    next.leftAside = value.leftAside
  }
  if (typeof value.pageSidebar === "boolean") {
    next.pageSidebar = value.pageSidebar
  }
  return next
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return Boolean(value) && typeof value === "object" && !Array.isArray(value)
}
