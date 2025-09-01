<template>
  <div class="flex flex-col divide-y divide-neutral-300 dark:divide-neutral-700 opacity-50">
    <ChannelPlaceholder
      v-for="i in channels"
      :key="`ph-${unitId}-${i}`"
      :index="i - 1"
      :type="resolveType(i - 1)"
    />
  </div>
</template>

<script setup lang="ts">
import ChannelPlaceholder from '@/components/ChannelPlaceholder.vue';

const props = defineProps<{
  unitId: string
  deviceType?: string
  channels: number
}>()

function resolveType(index: number): "DO" | "DI" | "AO" {
  // 📝 Здесь можно использовать deviceType или index, чтобы
  // назначить тип канала (DO/DI/AO). Например:
  if (props.deviceType === "do") return "DO"
  if (props.deviceType === "di") return "DI"
  if (props.deviceType === "ao") return "AO"
  return "DI"
}
</script>
