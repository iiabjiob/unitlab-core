import { defineStore } from "pinia"
import { ref } from "vue"

// Panel identifiers whose sizes we persist between sessions.
export type PanelSizeMap = {
  leftAside: number
  pageSidebar: number
  deviceChannels: number
  switchgearList: number
  sequenceSteps: number
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

  const STORAGE_KEY = "unitlab.ui"

  // Serialize current UI preferences into localStorage.
  function persist() {
    localStorage.setItem(
      STORAGE_KEY,
      JSON.stringify({
        panelSizes: panelSizes.value,
        collapsed: collapsed.value,
      })
    )
  }

  // Restore previously saved values, falling back to defaults if parsing fails.
  function restore() {
    const raw = localStorage.getItem(STORAGE_KEY)
    if (!raw) return

    try {
      const parsed = JSON.parse(raw)

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
    } catch (err) {
      console.warn("Failed to restore uiStore:", err)
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
