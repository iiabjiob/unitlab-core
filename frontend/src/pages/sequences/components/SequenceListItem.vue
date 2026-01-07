<script setup lang="ts">
import SidebarListItem from "@/components/ui/SidebarListItem.vue"
import type { SequenceDef } from "@/types/sequences"

const props = defineProps<{
  sequence: SequenceDef
  active: boolean
}>()

const emit = defineEmits<{ (e: "select", id: number): void }>()

function handleSelect() {
  emit("select", props.sequence.id)
}
</script>

<template>
  <SidebarListItem :active="active" @select="handleSelect">
    <span class="truncate text-sm">
      {{ sequence.name }}
    </span>
  </SidebarListItem>
</template>