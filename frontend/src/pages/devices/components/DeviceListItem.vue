<script setup lang="ts">
import SidebarListItem from "@/components/ui/SidebarListItem.vue"
import type { Device } from "@/types/device"
import { computed } from "vue"

const props = defineProps<{
  device: Device
  active: boolean
}>()

const emit = defineEmits<{ (e: "select", id: number): void }>()

const statusClass = computed(() => {
  switch (props.device.status) {
    case "online":
      return "bg-green-500"
    case "offline":
      return "bg-gray-600"
    default:
      return "bg-gray-600"
  }
})

function handleSelect() {
  emit("select", props.device.id)
}
</script>

<template>
  <SidebarListItem :active="active" @select="handleSelect">
    <template #prefix>
      <span class="w-2 h-2 rounded-full transition-colors" :class="statusClass" />
    </template>
    <span class="truncate text-sm">
      <p class="font-semibold text-sm text-neutral-900 dark:text-neutral-100 flex items-center gap-2">
        <span>{{ device.display_name }}</span>
        <span v-if="device.name" class="text-xs text-neutral-500 dark:text-neutral-400">· {{ device.unit_id }}</span>
      </p>
    </span>
    <template #suffix>
      <span class="text-xs truncate text-neutral-500 dark:text-neutral-400">
        {{ device.device_type }}
      </span>
    </template>
  </SidebarListItem>
</template>
