import { defineStore } from "pinia"
import { ref, computed } from "vue"
import type { Device } from "@/types/device"
import type { Switchgear } from "@/types/switchgear"
import type { Channel } from "@/types/channel"
import { useDeviceStore } from "@/stores/deviceStore"
import { useEditorStore } from "@/stores/editorStore"
import { useChannelStore } from "@/stores/channelStore"
import type { EntityMap, EntityType } from "@/types/entity"

// 3. SelectedEntity
export type SelectedEntity = {
  type: EntityType
  key: string | number
}

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

    if (type === "channel") {
      return selected.value.key === (item as Channel).id
    }
    if (type === "device") {
      return selected.value.key === (item as Device).unit_id
    }
    if (type === "switchgear") {
      return selected.value.key === (item as Switchgear).id
    }

    return false
  }

  // Получаем актуальный объект из стора
  const selectedItem = computed((): Channel | Device | Switchgear | undefined => {
    if (!selected.value) return undefined

    switch (selected.value.type) {
      case "channel": {
        const store = useChannelStore()
        return store.channels.find(c => c.id === selected.value!.key)
      }
      case "device": {
        const store = useDeviceStore()
        return store.devices.find(d => d.unit_id === selected.value!.key)
      }
      case "switchgear": {
        const editor = useEditorStore()
        return editor.items.find(s => s.id === selected.value!.key)
      }
    }
  })

  return { selected, select, clear, isSelected, selectedItem }
})
