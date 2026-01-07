<script setup lang="ts">
import SidebarListItem from "@/components/ui/SidebarListItem.vue"
import type { TestRunSummary } from "@/types/testRuns"

const props = defineProps<{ run: TestRunSummary; active: boolean }>()
const emit = defineEmits<{ (e: "select", id: number): void }>()

function handleSelect() {
  emit("select", props.run.id)
}
</script>

<template>
  <SidebarListItem :active="active" @select="handleSelect">
    <span class="truncate text-sm">
      {{ run.name }}
    </span>
  </SidebarListItem>
</template>
