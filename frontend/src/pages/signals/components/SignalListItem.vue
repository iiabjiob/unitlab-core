<script setup lang="ts">
import { computed } from "vue"
import type { SignalSnapshotSummary } from "@/types/signal"
import SidebarListItem from "@/components/ui/SidebarListItem.vue"
import UiBadge from "@/components/ui/UiBadge.vue"

const props = defineProps<{
  snapshot: SignalSnapshotSummary
  active: boolean
}>()

const emit = defineEmits<{ (e: "select", id: number): void }>()

const title = computed(() =>
  props.snapshot.source_filename ?? `Snapshot #${props.snapshot.id}`,
)

const meta = computed(() =>
  `${props.snapshot.rows_count} rows · schema v${props.snapshot.schema_version}`,
)

const statusVariant = computed(() =>
  props.snapshot.status === "locked" ? "success" : "warning",
)

function handleSelect() {
  emit("select", props.snapshot.id)
}
</script>

<template>
  <SidebarListItem :active="active" class="relative pr-16" @select="handleSelect">
    <span class="truncate text-sm font-medium">{{ title }}</span>
    <template #suffix>
      <span class="absolute right-3 top-1/2 -translate-y-1/2">
        <UiBadge :variant="statusVariant">
          {{ snapshot.status }}
        </UiBadge>
      </span>
    </template>
    <template #meta>
      {{ meta }}
    </template>
  </SidebarListItem>
</template>
