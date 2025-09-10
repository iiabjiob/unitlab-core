import { defineStore } from "pinia"
import { ref, computed } from "vue"
import type { Device } from "@/types/device"
import type { Switchgear } from "@/types/switchgear"
import { useDeviceStore } from "@/stores/deviceStore"
import { useEditorStore } from "@/stores/editorStore"

// 1. Определяем доступные типы
export type EntityType = "device" | "switchgear" // расширяй: | "sequence" | "channel" ...

// 2. Карта типов → сущности
export type EntityMap = {
  device: Device
  switchgear: Switchgear
  // sequence: Sequence
  // channel: Channel
}

// 3. SelectedEntity всегда { type, id }
export type SelectedEntity = {
  [K in EntityType]: { type: K; id: string }
}[EntityType]

export const useSelectionStore = defineStore("selectionStore", () => {
  const selected = ref<SelectedEntity | null>(null)

  function select(entity: SelectedEntity) {
    selected.value = entity
  }

  function clear() {
    selected.value = null
  }

  function isSelected<T extends EntityType>(type: T, item: EntityMap[T]) {
    if (selected.value?.type !== type) return false

    if (type === "device") {
      return String(selected.value.id) === String((item as Device).unit_id)
    }
    if (type === "switchgear") {
      return String(selected.value.id) === String((item as Switchgear).id)
    }

    return false
  }

  // 4. Получаем актуальный объект из стора
  const selectedItem = computed((): Device | Switchgear | undefined => {
    if (!selected.value) return undefined

    switch (selected.value.type) {
      case "device": {
        const store = useDeviceStore()
        return store.devices.find(d => d.unit_id === selected.value!.id)
      }
      case "switchgear": {
        const editor = useEditorStore()
        return editor.items.find(s => s.id === selected.value!.id)
      }
    }
  })

  return { selected, select, clear, isSelected, selectedItem }
})
