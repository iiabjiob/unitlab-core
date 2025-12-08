<script setup lang="ts">
import { computed } from "vue"
import { useChannelStore } from "@/stores/channelStore"
import DeviceChannelItem from "./DeviceChannelItem.vue"
import type { Device } from "@/types/device";

const props = defineProps<{ device: Device }>()

const channelStore = useChannelStore()

const channels = computed(() =>
  channelStore.channelsByDevice(props.device.id)
)

</script>

<template>
  <div class="flex flex-col h-full overflow-hidden p-4">

    <!-- HEADER -->
    <div class="text-xs uppercase tracking-wide text-neutral-500 dark:text-neutral-400">
      Channels ({{ channels.length }})
    </div>

    <!-- CONTROL TOOLBAR -->
    <div class="py-2">
      <!-- <DeviceChannelsControlToolbar
        
      /> -->
    </div>

    <!-- LIST -->
    <div class="flex-1 overflow-y-auto divide-y divide-neutral-300 dark:divide-neutral-700">
      <DeviceChannelItem
        v-for="channel in channels"
        :key="channel.id"
        :channel="channel"
      />
    </div>

  </div>
</template>
