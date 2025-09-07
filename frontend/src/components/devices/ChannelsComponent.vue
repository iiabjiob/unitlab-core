<template>
  <div
    class="flex flex-col divide-y divide-neutral-300 dark:divide-neutral-700"
    :class="{ 'opacity-50 pointer-events-none': disabled }"
  >
    <!-- Реальные каналы -->
    <ChannelComponent
      v-for="ch in realChannels"
      :key="`${unitId}-${ch.index}`"
      :channel="ch"
      :type="deviceType"
      @toggle="state => onToggle(ch, state)"
      @ao-change="val => onAoChange(ch, val)"
    />
  </div>
</template>

<script setup lang="ts">
import { computed } from "vue"
import { useChannelStore } from "@/stores/channelStore"
import ChannelComponent from "./ChannelComponent.vue"
import type { Channel, ChannelType } from "@/types/channel"

const props = defineProps<{
  unitId: string
  deviceType: ChannelType
  channels?: number
  disabled?: boolean
}>()

const channelStore = useChannelStore()

// реальные каналы
const realChannels = computed<Channel[]>(() => {
  return channelStore.channels[props.unitId] || []
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
