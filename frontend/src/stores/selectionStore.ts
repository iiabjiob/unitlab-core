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
import { SCHEMA_NAMES } from "@/property-schemas/types"

// 3. SelectedEntity
export type SelectedEntity = {
  type: EntityType
  key: string | number
}

// stores/selectionStore.ts
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

    if (type === SCHEMA_NAMES.CHANNEL) {
      return selected.value.key === (item as Channel).id
    }
    if (type === SCHEMA_NAMES.DEVICE) {
      return selected.value.key === (item as Device).unit_id
    }
    if (type === SCHEMA_NAMES.SWITCHGEAR) {
      return selected.value.key === (item as Switchgear).id
    }
    if (type === SCHEMA_NAMES.SEQUENCE) {
      return selected.value.key === (item as SequenceDef).id
    }
    if (type === SCHEMA_NAMES.SEQUENCE_STEP) {
      return selected.value.key === (item as SequenceStep).id
    }

    return false
  }

  const selectedItem = computed((): Channel | Device | Switchgear | SequenceDef | SequenceStep | undefined => {
    if (!selected.value) return undefined

    switch (selected.value.type) {
      case SCHEMA_NAMES.CHANNEL: {
        const store = useChannelStore()
        return store.channels.find(c => c.id === selected.value!.key)
      }
      case SCHEMA_NAMES.DEVICE: {
        const store = useDeviceStore()
        return store.devices.find(d => d.unit_id === selected.value!.key)
      }
      case SCHEMA_NAMES.SWITCHGEAR: {
        const store = useSwitchgearStore()
        return store.switchgears.find(s => s.id === selected.value!.key)
      }
      case SCHEMA_NAMES.SEQUENCE: {
        const store = useSequenceStore()
        return store.sequences.find(s => s.id === selected.value!.key)
      }
      case SCHEMA_NAMES.SEQUENCE_STEP: {
        const store = useSequenceStore()
        for (const seq of store.sequences) {
          const step = seq.steps.find(st => (st as any).id === selected.value!.key)
          if (step) return step
        }
      }
    }
  })

  /**
   * Проверяет, должен ли клик сохранить выделение
   */
  function shouldKeepSelection(target: HTMLElement) {
    // если внутри карточки
    if (target.closest(".selectable-card")) return true
    if (target.closest(".selectable-row")) return true

    // если внутри панели с игнорированием
    if (target.closest(".ignore-selection")) return true

    return false
  }

  return { selected, select, clear, isSelected, selectedItem, shouldKeepSelection }
})
