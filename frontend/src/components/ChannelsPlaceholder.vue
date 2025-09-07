<template>
  <div class="flex flex-col divide-y divide-neutral-300 dark:divide-neutral-700 opacity-50">
    <ChannelPlaceholder
      v-for="i in channels"
      :key="`ph-${unitId}-${i}`"
      :index="i - 1"
      :type="deviceType"
    />
  </div>
</template>

<script setup lang="ts">
import ChannelPlaceholder from '@/components/ChannelPlaceholder.vue';
import type { ChannelType } from '@/types/channel';

const props = defineProps<{
  unitId: string
  deviceType: ChannelType
  channels: number
}>()

</script>
