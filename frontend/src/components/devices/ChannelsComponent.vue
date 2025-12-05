<template>
  <div
    v-if="disabled"
    class="channel-strip channel-strip--placeholder flex w-full gap-2 pb-2 pt-1"
  >
    <span
      v-for="idx in placeholderCount"
      :key="idx"
      class="channel-placeholder"
    />
  </div>
  <div
    v-else
    class="channel-strip flex w-full gap-2 overflow-x-auto pb-2 pt-1"
  >
    <ChannelComponent
      v-for="ch in channels"
      :key="`${unitId}-${ch.index}`"
      :channel="ch"
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

const props = defineProps<{
  unitId: string
  deviceType: ChannelType
  channels?: number
  disabled?: boolean
}>()

const channelStore = useChannelStore()
const deviceStore = useDeviceStore()
// каналы (ищем device.id по unitId)
const channels = computed<Channel[]>(() => {
  const dev = deviceStore.devices.find(d => d.unit_id === props.unitId)
  if (!dev) return []
  return channelStore.channels.filter(c => c.device_id === dev.id)
})

const placeholderCount = computed(() => {
  if (props.channels && props.channels > 0) {
    return Math.min(props.channels, 12)
  }
  if (channels.value.length) {
    return Math.min(channels.value.length, 12)
  }
  return 6
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

<style scoped>
.channel-strip {
  max-width: 100%;
  overflow-x: auto;
  overscroll-behavior-x: contain;
}

.channel-strip::-webkit-scrollbar {
  height: 6px;
}

.channel-strip::-webkit-scrollbar-track {
  background: transparent;
}

.channel-strip::-webkit-scrollbar-thumb {
  background: rgba(100, 116, 139, 0.4);
  border-radius: 9999px;
}

:global(.dark) .channel-strip::-webkit-scrollbar-thumb {
  background: rgba(71, 85, 105, 0.8);
}

.channel-strip--placeholder {
  overflow: hidden;
}

.channel-placeholder {
  width: 1.6rem;
  height: 1.6rem;
  border-radius: 9999px;
  background: linear-gradient(90deg, rgba(148, 163, 184, 0.25), rgba(148, 163, 184, 0.4), rgba(148, 163, 184, 0.25));
  background-size: 200% 100%;
  animation: pulse 1.5s ease-in-out infinite;
}

:global(.dark) .channel-placeholder {
  background: linear-gradient(90deg, rgba(71, 85, 105, 0.35), rgba(148, 163, 184, 0.45), rgba(71, 85, 105, 0.35));
}

@keyframes pulse {
  0% {
    background-position: 0% 50%;
    opacity: 0.4;
  }
  50% {
    background-position: 100% 50%;
    opacity: 0.8;
  }
  100% {
    background-position: 0% 50%;
    opacity: 0.4;
  }
}
</style>
