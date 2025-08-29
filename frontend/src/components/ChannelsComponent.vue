<template>
  <div class="flex flex-col divide-y divide-gray-300 dark:divide-gray-700">
    <!-- Реальные каналы -->
    <ChannelComponent
      v-for="ch in realChannels"
      :key="`${unitId}-${ch.index}`"
      :channel="ch"
      @toggle="state => onToggle(ch, state)"
      @ao-change="val => onAoChange(ch, val)"
    />

    <!-- Заглушки -->
    <ChannelComponent
      v-for="ch in placeholderChannels"
      :key="`${unitId}-ph-${ch.index}`"
      :channel="ch"
      :disabled="true"
      class="opacity-50"
    />
  </div>
</template>

<script setup lang="ts">
import { computed } from "vue"
import { useChannelStore } from "@/stores/channelStore"
import { useDeviceStore } from "@/stores/deviceStore"
import ChannelComponent from "./ChannelComponent.vue"
import type { Channel } from "@/types/channel"

const props = defineProps<{
  unitId: string
  deviceType?: string
  channels?: number
}>()

const channelStore = useChannelStore()
const deviceStore = useDeviceStore()

// реальные каналы
const realChannels = computed<Channel[]>(() => {
  return channelStore.channels[props.unitId] || []
})

// заглушки
const placeholderChannels = computed<Channel[]>(() => {
  if (realChannels.value.length > 0) return []

  // пробуем взять из props → иначе из deviceStore
  const devType = props.deviceType ?? deviceStore.devices.find(d => d.unit_id === props.unitId)?.device_type ?? "DI"
  const totalChannels = props.channels ?? deviceStore.devices.find(d => d.unit_id === props.unitId)?.channels ?? 0

  const mock: Channel[] = []
  for (let i = 0; i < totalChannels; i++) {
    if (devType.toUpperCase() === "DO") {
      mock.push({ index: i, device_id: props.unitId, type: "DO", state: false, name: `DO${i+1}` })
    } else if (devType.toUpperCase() === "AO") {
      mock.push({ index: i, device_id: props.unitId, type: "AO", state: 4+i, name: `AO${i+1}` })
    } else {
      mock.push({ index: i, device_id: props.unitId, type: "DI", state: i % 2 === 0, name: `DI${i+1}` })
    }
  }
  return mock
})

// обработчики
function onToggle(ch: Channel, state: boolean) {
  if (ch.type === "DO") {
    channelStore.sendDoCommand(props.unitId, ch.index, state)
  }
}
function onAoChange(ch: Channel, value: number) {
  if (ch.type === "AO") {
    channelStore.sendAoCommand(props.unitId, ch.index, value)
  }
}
</script>
