<script setup lang="ts">
import SidebarListItem from "@/components/ui/SidebarListItem.vue"
import type { Switchgear } from "@/types/switchgear"

const props = defineProps<{
  switchgear: Switchgear
  active: boolean
}>()

const emit = defineEmits<{ (e: "select", id: number): void }>()

function handleSelect() {
  emit("select", props.switchgear.id)
}
</script>

<template>
  <SidebarListItem :active="active" @select="handleSelect">
    <span class="truncate text-sm font-medium">
      {{ switchgear.name }}
    </span>
    <template #subtitle>
      {{ switchgear.switchgear_type }}
    </template>
  </SidebarListItem>
</template>
