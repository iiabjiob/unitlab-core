<template>
  <div
    class="flex flex-col divide-y divide-neutral-300 dark:divide-neutral-700"
    :class="{ 'opacity-50 pointer-events-none': disabled }"
  >

      <ChannelComponent
        v-for="ch in realChannels"
        :key="`${unitId}-${ch.index}`"
        :channel="ch"
        :type="deviceType"
        :selected="selection.isSelected('channel', ch)"
        @select="select(ch)"
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

function select(item: Channel) {
  selection.select({ type: "channel", key: item.id })
}

// реальные каналы (ищем device.id по unitId)
const realChannels = computed<Channel[]>(() => {
  const dev = deviceStore.devices.find(d => d.unit_id === props.unitId)
  console.log("🔍 realChannels: dev=", dev, "channels=", channelStore.channels)
  if (!dev) return []
  return channelStore.channels.filter(c => c.device_id === dev.id)
})

// обработчики
function onToggle(ch: Channel, state: boolean) {
  if (props.disabled) return
  if (props.deviceType === "DO") {
    channelStore.sendDoCommand(props.unitId, ch.index, state)
  }
}
function onAoChange(ch: Channel, value: number) {
  if (props.disabled) return
  if (props.deviceType === "AO") {
    channelStore.sendAoCommand(props.unitId, ch.index, value)
  }
}
</script>
