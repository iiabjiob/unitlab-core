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

const isOffline = computed(() => props.device.status !== "online")

const channelCount = computed(() => channels.value.length)

const hasDoChannels = computed(() =>
  channels.value.length > 0 &&
  channels.value.some(ch => ch.type === "do")
)

function onToggle(ch: Channel) {
  const next = !ch.state
  channelStore.sendDoCommand(
    channelStore.resolveUnitId(ch.device_id),
    ch.index,
    next
  )
}

function onSetAo({ channel, value }: { channel: Channel; value: number }) {
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

    <!-- OFFLINE STATE -->
    <div
      v-if="isOffline"
      class="flex-1 flex items-center justify-center text-neutral-500 text-sm italic"
    >
      Device is offline — channel states unavailable
    </div>

    <!-- ONLINE STATE -->
    <template v-else>
      <!-- CONTROL TOOLBAR -->
      <div v-if="hasDoChannels" class="py-2">
        <DeviceChannelControlToolbar
          :device-id="device.id"
          :unit-id="device.unit_id"
        />
      </div>

      <!-- LIST -->
      <div class="grid grid-cols-4 gap-2 mt-5">
        <DeviceChannelItem
          v-for="channel in channels"
          :key="channel.id"
          :channel="channel"
          @toggle="onToggle"
          @set-ao="onSetAo"
        />
      </div>
    </template>

  </div>
</template>

