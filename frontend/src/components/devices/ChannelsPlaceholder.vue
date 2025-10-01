<template>
  <div class="flex flex-col divide-y divide-neutral-300 dark:divide-neutral-700 opacity-50">
    <ChannelPlaceholder
      v-for="ch in channels"
      :key="`${unitId}-${ch.index}`"
      :channel="ch"
    />
  </div>
</template>

<script setup lang="ts">
import ChannelPlaceholder from '@/components/devices/ChannelPlaceholder.vue';
import { useChannelStore } from '@/stores/channelStore';
import { useDeviceStore } from '@/stores/deviceStore';
import type { Channel, ChannelType } from '@/types/channel';
import { computed } from 'vue';

const channelStore = useChannelStore()
const deviceStore = useDeviceStore()

const channels = computed<Channel[]>(() => {
  const dev = deviceStore.devices.find(d => d.unit_id === props.unitId)
  if (!dev) return []
  return channelStore.channels.filter(c => c.device_id === dev.id)
})

const props = defineProps<{
  unitId: string
  deviceType: ChannelType
  num_channels: number
}>()

</script>
