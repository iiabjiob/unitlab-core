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
    ? "mt-5 flex min-h-0 flex-col gap-1.5 overflow-y-auto pr-1"
    : "mt-5 grid min-h-0 grid-flow-col grid-rows-8 content-start gap-x-1 gap-y-2 overflow-y-auto pr-1"
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
  <div class="flex flex-col h-full overflow-hidden p-4">

    <!-- HEADER -->
    <div class="text-xs uppercase tracking-wide text-neutral-500 dark:text-neutral-400">
      Channels ({{ channelCount }})
    </div>

    <div
      v-if="isOffline"
      class="mt-2 rounded border border-amber-300/70 bg-amber-50 px-2 py-1 text-xs text-amber-700 dark:border-amber-700/60 dark:bg-amber-900/20 dark:text-amber-300"
    >
      Device is offline: controls are disabled until it is back online.
    </div>

    <!-- CONTROL TOOLBAR -->
    <div v-if="hasDoChannels" class="py-2">
      <DeviceChannelControlToolbar
        :device-id="device.id"
        :unit-id="device.unit_id"
        :disabled="isOffline"
      />
    </div>

    <!-- LIST -->
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
