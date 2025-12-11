import { defineStore } from "pinia"
import { ref } from "vue"

// Типы панелей, которые можно ресайзить
export type PanelSizeMap = {
  leftAside: number
  pageSidebar: number
  deviceChannels: number
  switchgearList: number
  sequenceSteps: number
}

export const useUiStore = defineStore("uiStore", () => {
  // --- STATE --------------------------------------------------------

  // Все размеры панелей храним в одном объекте (удобно расширять и сохранять)
  const panelSizes = ref<PanelSizeMap>({
    leftAside: 280,
    pageSidebar: 300,
    deviceChannels: 600,
    switchgearList: 620,
    sequenceSteps: 720,
  })

  // Например collapsed-состояния
  const collapsed = ref({
    leftAside: false,
    pageSidebar: false,
  })

  // --- ACTIONS -------------------------------------------------------

  function setPanelSize(panel: keyof PanelSizeMap, value: number) {
    panelSizes.value[panel] = value
    persist() // сохраняем сами, без magic Pinia persist
  }

  function toggleCollapse(panel: keyof typeof collapsed.value) {
    collapsed.value[panel] = !collapsed.value[panel]
    persist()
  }

  // --- PERSISTENCE ---------------------------------------------------

  const STORAGE_KEY = "unitlab.ui"

  function persist() {
    localStorage.setItem(
      STORAGE_KEY,
      JSON.stringify({
        panelSizes: panelSizes.value,
        collapsed: collapsed.value,
      })
    )
  }

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
