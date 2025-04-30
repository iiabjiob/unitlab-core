<template>
  <div class="flex flex-col sm:flex-row sm:items-center sm:justify-between p-4 border border-gray-200 rounded dark:border-gray-800 bg-white dark:bg-gray-800 gap-2">
    <label class="flex items-center gap-2">
      <input
        type="checkbox"
        :checked="selected"
        :id="`select-${device.unit_id}`"
        :name="`select-${device.unit_id}`"
        @change="$emit('select', device.unit_id)"
        class="accent-gray-500"
      />
      <span class="font-mono font-semibold">{{ device.unit_id }}</span>
    </label>

    <span class="text-sm text-gray-500 dark:text-gray-400">
      Type: {{ device.type }}
    </span>

    <div class="min-w-[100px] flex justify-start md:justify-center">
      <BadgeComponent  :variant="device.is_active ? 'success' : 'danger'">
        {{ device.is_active ? 'Active' : 'Inactive' }}
      </BadgeComponent>
    </div>

    <DeviceActions
      :is-active="device.is_active"
      @toggle="$emit('toggle')"
      @delete="$emit('delete')"
    />
  </div>
</template>

<script setup lang="ts">
import type { Device } from '@/types/device'
import BadgeComponent from './ui/BadgeComponent.vue'
import DeviceActions from './DeviceActions.vue'

defineProps<{
  device: Device
  selected: boolean
}>()

defineEmits(['toggle', 'delete', 'select'])
</script>
