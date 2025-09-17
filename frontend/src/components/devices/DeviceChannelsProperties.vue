<!-- src/components/devices/DeviceChannelsProperties.vue -->
<template>
  <div class="mt-2">
    <h4 class="font-bold text-sm">Channels</h4>

    <PropertiesPanel
      v-for="ch in channels"
      :key="ch.id"
      :schema="channelPropertySchema"
      :item="ch"
      :item-id="ch.id"
      @update="(key, value) => channelPropertySchema.update(ch, key as keyof Channel, value)"
    />
  </div>
</template>

<script setup lang="ts">
import { computed } from "vue"
import { channelPropertySchema } from "@/property-schemas/channelPropertySchema"
import { useChannelStore } from "@/stores/channelStore"
import type { Device } from "@/types/device"
import PropertiesPanel from "../PropertiesPanel.vue";
import type { Channel } from "@/types/channel";

const props = defineProps<{
  device: Device
}>()

const channelStore = useChannelStore()
const channels = computed(() =>
  channelStore.channels.filter(ch => ch.device_id === props.device.id)
)
</script>
