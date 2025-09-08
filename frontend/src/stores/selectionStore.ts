import { defineStore } from "pinia"
import { ref } from "vue"
import type { Device } from "@/types/device"

type SelectedEntity =
  | { type: "device"; item: Device }
  | null

export const useSelectionStore = defineStore("selectionStore", () => {
  const selected = ref<SelectedEntity>(null)

  function select(entity: SelectedEntity) {
    selected.value = entity
  }

  function clear() {
    selected.value = null
  }

  return { selected, select, clear }
})
