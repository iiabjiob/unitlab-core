<script setup lang="ts">
import { computed } from "vue"
import type { SignalSnapshotSummary } from "@/types/signal"
import UiBadge from "@/components/ui/UiBadge.vue"
import UiButton from "@/components/ui/UiButton.vue"
import {
  UiMenu,
  UiMenuTrigger,
  UiMenuContent,
  UiMenuItem,
} from "@affino/menu-vue"
import EllipsisHorizontalIcon from "@/components/icons/EllipsisHorizontalIcon.vue"
import { formatDate } from "@/utils/datetime"

const props = defineProps<{
  snapshot: SignalSnapshotSummary
}>()

const emit = defineEmits<{
  (e: "lock"): void
  (e: "delete"): void
}>()

const title = computed(() =>
  props.snapshot.source_filename ?? `Snapshot #${props.snapshot.id}`,
)

const statusVariant = computed(() =>
  props.snapshot.status === "locked" ? "success" : "warning",
)

const createdAt = computed(() => formatDate(props.snapshot.created_at))
const lockedAt = computed(() =>
  props.snapshot.locked_at ? formatDate(props.snapshot.locked_at) : null,
)
const shortHash = computed(() =>
  props.snapshot.source_hash ? props.snapshot.source_hash.slice(0, 8) : null,
)

const statusLine = computed(() =>
  lockedAt.value ? `Locked ${lockedAt.value}` : `Imported ${createdAt.value}`,
)
</script>

<template>
  <div class="px-4 py-3 flex items-start justify-between border-b border-neutral-300 dark:border-neutral-800">
    <div class="flex items-start gap-3">
      <div class="flex flex-col gap-1">
        <div class="text-lg font-medium tracking-tight text-neutral-900 dark:text-white">
          {{ title }}
        </div>

        <div class="flex flex-wrap items-center gap-3 text-sm text-neutral-500 dark:text-neutral-400">
          <span class="text-xs uppercase tracking-[0.3em] text-neutral-400 dark:text-neutral-500">
            Snapshot
          </span>
          <span>·</span>
          <span>{{ snapshot.rows_count }} rows</span>
          <span>·</span>
          <span>Schema v{{ snapshot.schema_version }}</span>
          <span>·</span>
          <UiBadge :variant="statusVariant">
            {{ snapshot.status }}
          </UiBadge>
        </div>

        <div class="text-xs text-neutral-500 dark:text-neutral-400">
          <span>{{ statusLine }}</span>
          <template v-if="shortHash">
            <span>·</span>
            <span>Hash {{ shortHash }}</span>
          </template>
        </div>
      </div>
    </div>

    <UiMenu>
      <UiMenuTrigger asChild>
        <UiButton variant="icon" aria-label="Snapshot actions">
          <EllipsisHorizontalIcon size="24" />
        </UiButton>
      </UiMenuTrigger>

      <UiMenuContent>
        <UiMenuItem v-if="snapshot.status !== 'locked'" class="text-neutral-900 dark:text-neutral-200" @select="emit('lock')">
          Lock snapshot
        </UiMenuItem>
        <UiMenuItem danger @select="emit('delete')">
          Delete
        </UiMenuItem>
      </UiMenuContent>
    </UiMenu>
  </div>
</template>
