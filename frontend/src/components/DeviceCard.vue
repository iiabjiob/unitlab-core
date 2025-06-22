<template>
  <div
    class="rounded shadow bg-white dark:bg-gray-900 p-4 flex flex-col gap-3 transition-opacity duration-200"
    :class="{
      'opacity-40 pointer-events-none select-none': !device.is_online, // офлайн
      'bg-gray-50 dark:bg-gray-800': !device.is_active,               // неактивно
    }"
  >
    <!-- Header -->
    <div class="flex flex-wrap items-center justify-between mb-2">
      <div class="flex items-center gap-2">
        <span class="ml-2 text-xs font-mono">
          {{ device.is_online ? '🟢' : '⚪️' }}
        </span>
        <span class="font-bold text-base">{{ device.name || device.unit_id }}</span>
        <BadgeComponent>{{ device.type.toUpperCase() }}</BadgeComponent>
      </div>
      <DeviceActionsMenu
        :is-active="device.is_active"
        @toggle="() => $emit('toggle', device.unit_id)"
        @delete="() => $emit('delete', device.unit_id)"
      />
    </div>

    <!-- Channels list -->
    <div v-if="channels.length" class="flex flex-col gap-2">
      <DeviceChannelRow
        v-for="channel in channels"
        :key="channel.index"
        :channel="channel"
        :disabled="!device.is_active || !device.is_online"
        @toggle="() => $emit('toggle-channel', device.unit_id, channel.index)"
        @pulse="() => $emit('pulse-channel', device.unit_id, channel.index)"
      />
    </div>
    <div v-else class="text-gray-400 text-xs">No channels found for this device.</div>
  </div>
</template>

<script setup lang="ts">
import DeviceActionsMenu from './DeviceActionsMenu.vue'
import DeviceChannelRow from './DeviceChannelRow.vue'
import type { Device } from '@/types/device'
import type { Channel } from '@/types/channel'
import BadgeComponent from './ui/BadgeComponent.vue';

defineProps<{
  device: Device
  channels: Channel[]
}>()

defineEmits([
  'toggle',
  'delete',
  'toggle-channel',
  'pulse-channel'
])
</script>
