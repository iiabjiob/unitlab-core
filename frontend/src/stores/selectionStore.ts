import { defineStore } from "pinia"
import { ref, computed } from "vue"
import type { Device } from "@/types/device"
import type { Switchgear } from "@/types/switchgear"
import type { Channel } from "@/types/channel"
import { useDeviceStore } from "@/stores/deviceStore"

import { useChannelStore } from "@/stores/channelStore"
import type { EntityMap, EntityType } from "@/types/entity"
import { useSwitchgearStore } from "./switchgearStore"
import type { SequenceDef, SequenceStep } from "@/types/sequences"
import { useSequenceStore } from "./sequenceStore"

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

    if (type === "sequence") {
      return selected.value.key === (item as SequenceDef).id
    }
    if (type === "sequence_step") {
      // шаги обычно идентифицируем по id из БД или по индексу
      return selected.value.key === (item as SequenceStep).id
    }

    return false
  }

  // Получаем актуальный объект из стора
  const selectedItem = computed((): Channel | Device | Switchgear | SequenceDef | SequenceStep | undefined => {
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
        const store = useSwitchgearStore()
        return store.switchgears.find(s => s.id === selected.value!.key)
      }
      case "sequence": {
        const store = useSequenceStore()
        return store.sequences.find(s => s.id === selected.value!.key)
      }
      case "sequence_step": {
        const store = useSequenceStore()
        for (const seq of store.sequences) {
          const step = seq.steps.find(st => (st as any).id === selected.value!.key)
          if (step) return step
        }
      }
    }
  })

  return { selected, select, clear, isSelected, selectedItem }
})
