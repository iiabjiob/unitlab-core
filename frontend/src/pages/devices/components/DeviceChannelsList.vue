<script setup lang="ts">
import { computed } from "vue"
import { useChannelStore } from "@/stores/channelStore"
import DeviceChannelItem from "./DeviceChannelItem.vue"
import DeviceChannelControlToolbar from "./DeviceChannelControlToolbar.vue"
import type { Device } from "@/types/device"
import type { Channel } from "@/types/channel"

const props = defineProps<{ device: Device }>()

const channelStore = useChannelStore()

const channels = computed(() =>
  channelStore.channelsByDevice(props.device.id)
)

const orderedChannels = computed(() =>
  [...channels.value].sort((left, right) => left.index - right.index)
)

const isOffline = computed(() => props.device.status !== "online")

const channelCount = computed(() => channels.value.length)
const deviceChannelType = computed(() => props.device.device_type)
const digitalColumnCount = computed(() => Math.max(1, Math.ceil(orderedChannels.value.length / 8)))

const channelListClass = computed(() => (
  deviceChannelType.value === "ao"
    ? "device-channels-list__items device-channels-list__items--analog"
    : "device-channels-list__items device-channels-list__items--digital"
))

const channelListStyle = computed(() => {
  if (deviceChannelType.value === "ao") {
    return undefined
  }

  return {
    gridTemplateColumns: `repeat(${digitalColumnCount.value}, minmax(0, 1fr))`,
  }
})

const hasDoChannels = computed(() =>
  channels.value.length > 0 &&
  channels.value.some(ch => ch.type === "do")
)

function onToggle(ch: Channel) {
  if (isOffline.value) {
    return
  }
  const next = !ch.state
  channelStore.sendDoCommand(
    channelStore.resolveUnitId(ch.device_id),
    ch.index,
    next
  )
}

function onSetAo({ channel, value }: { channel: Channel; value: number }) {
  if (isOffline.value) {
    return
  }
  channelStore.sendAoCommand(
    channelStore.resolveUnitId(channel.device_id),
    channel.index,
    value
  )
}

</script>

<template>
  <div class="device-channels-list">

    <div class="device-channels-list__header">
      Channels ({{ channelCount }})
    </div>

    <div
      v-if="isOffline"
      class="device-channels-list__offline"
    >
      Device is offline: controls are disabled until it is back online.
    </div>

    <div v-if="hasDoChannels" class="device-channels-list__toolbar">
      <DeviceChannelControlToolbar
        :device-id="device.id"
        :unit-id="device.unit_id"
        :disabled="isOffline"
      />
    </div>

    <div :class="channelListClass" :style="channelListStyle">
      <DeviceChannelItem
        v-for="channel in orderedChannels"
        :key="channel.id"
        :channel="channel"
        :device-type="deviceChannelType"
        :disabled="isOffline"
        @toggle="onToggle"
        @set-ao="onSetAo"
      />
    </div>

  </div>
</template>

<style scoped>
.device-channels-list {
  display: flex;
  height: 100%;
  min-height: 0;
  flex-direction: column;
  overflow: hidden;
  padding: 1rem;
}

.device-channels-list__header {
  flex: 0 0 auto;
  color: var(--color-neutral-500);
  font-size: var(--text-xs);
  letter-spacing: 0.025em;
  text-transform: uppercase;
}

.device-channels-list__offline {
  flex: 0 0 auto;
  margin-top: 0.5rem;
  padding: 0.25rem 0.5rem;
  border: 1px solid color-mix(in srgb, var(--color-amber-300) 70%, transparent);
  border-radius: var(--radius-sm);
  background: var(--color-amber-50);
  color: var(--color-amber-700);
  font-size: var(--text-xs);
}

.device-channels-list__toolbar {
  flex: 0 0 auto;
  padding: 0.5rem 0;
}

.device-channels-list__items {
  flex: 1 1 auto;
  min-height: 0;
  margin-top: 1.25rem;
  overflow-y: auto;
  padding-right: 0.25rem;
}

.device-channels-list__items--analog {
  display: flex;
  flex-direction: column;
  gap: 0.375rem;
}

.device-channels-list__items--digital {
  display: grid;
  grid-auto-flow: column;
  grid-template-rows: repeat(8, minmax(0, 1fr));
  align-content: start;
  column-gap: 0.25rem;
  row-gap: 0.5rem;
}

:global(.dark .device-channels-list__header) {
  color: var(--color-neutral-400);
}

:global(.dark .device-channels-list__offline) {
  border-color: color-mix(in srgb, var(--color-amber-700) 60%, transparent);
  background: color-mix(in srgb, var(--color-amber-900) 20%, transparent);
  color: var(--color-amber-300);
}
</style>
