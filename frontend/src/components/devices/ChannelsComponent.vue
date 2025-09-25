<template>
  <div
    class="flex flex-col divide-y divide-neutral-300 dark:divide-neutral-700"
    :class="{ 'opacity-50 pointer-events-none': disabled }"
  >

      <ChannelComponent
        v-for="ch in channels"
        :key="`${unitId}-${ch.index}`"
        :channel="ch"
        :selected="selection.isSelected('channel', ch)"
        @toggle="state => onToggle(ch, state)"
        @ao-change="val => onAoChange(ch, val)"
      />

  </div>
</template>

<script setup lang="ts">
import { computed } from "vue"
import { useChannelStore } from "@/stores/channelStore"
import { useDeviceStore } from "@/stores/deviceStore"
import ChannelComponent from "./ChannelComponent.vue"
import type { Channel, ChannelType } from "@/types/channel"
import { useSelectionStore } from "@/stores/selectionStore"

const props = defineProps<{
  unitId: string
  deviceType: ChannelType
  channels?: number
  disabled?: boolean
}>()

const channelStore = useChannelStore()
const deviceStore = useDeviceStore()
const selection = useSelectionStore()

// каналы (ищем device.id по unitId)
const channels = computed<Channel[]>(() => {
  const dev = deviceStore.devices.find(d => d.unit_id === props.unitId)
  if (!dev) return []
  return channelStore.channels.filter(c => c.device_id === dev.id)
})

// обработчики
function onToggle(ch: Channel, state: boolean) {
  if (props.disabled) return
  if (props.deviceType === "do") {
    channelStore.sendDoCommand(props.unitId, ch.index, state)
  }
}
function onAoChange(ch: Channel, value: number) {
  if (props.disabled) return
  if (props.deviceType === "ao") {
    channelStore.sendAoCommand(props.unitId, ch.index, value)
  }
}
</script>
