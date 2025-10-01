<template>
  <div class="flex flex-col h-full">
    <!-- Верхняя строка -->
    <div class="flex items-center justify-between">
      <!-- ID/Name + статус + location -->
      <div class="flex items-center gap-2 flex-wrap">

        <span class="font-mono font-semibold">
          {{ device.name?.trim() || device.unit_id }}
        </span>

        <OnlineStatusComponent :status="device.status"/>

      </div>

      <DeviceMenu
        :is-active="device.is_active"
        @toggle="$emit('toggle', device)"
        @delete="$emit('delete', device)"
      />
    </div>
    <!-- Location (optional) -->
    <p class="text-xs text-neutral-500" v-if="device.location">Location: {{ device.location }}</p>

    <!-- Каналы -->
    <div class="mt-3">
      <ChannelsComponent
        v-if="device.status === 'online'"
        :unit-id="device.unit_id"
        :device-type="device.type"
        :num_channels="device.num_channels"
      />

      <ChannelsPlaceholder
        v-else
        :unit-id="device.unit_id"
        :device-type="device.type"
        :num_channels="device.num_channels"
      />
    </div>
  </div>
</template>

<script setup lang="ts">
import type { Device } from "@/types/device"
import ChannelsComponent from "./ChannelsComponent.vue"
import OnlineStatusComponent from "../misc/OnlineStatusComponent.vue"
import ChannelsPlaceholder from "./ChannelsPlaceholder.vue"
import DeviceMenu from "./DeviceMenu.vue"

const props = defineProps<{
  device: Device
}>()

const emit = defineEmits<{
  (e: 'toggle', device: Device): void
  (e: 'delete', device: Device): void
}>()

</script>
