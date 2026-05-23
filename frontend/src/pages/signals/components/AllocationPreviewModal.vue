<template>
  <UiModal :open="open" :title="title" maxWidthClass="max-w-4xl" @close="emit('close')">
    <div v-if="loading" class="py-10 text-center text-sm text-neutral-500 dark:text-neutral-400">
      Preparing preview...
    </div>

    <div v-else class="space-y-4">
      <div
        v-if="error"
        class="rounded-lg border border-rose-200 bg-rose-50 px-3 py-2 text-sm text-rose-700 dark:border-rose-900/60 dark:bg-rose-950/30 dark:text-rose-200"
      >
        {{ error }}
      </div>

      <template v-if="preview">
        <div class="grid gap-2 sm:grid-cols-3 lg:grid-cols-6">
          <div v-for="item in summaryItems" :key="item.key" class="rounded-lg border border-neutral-200 bg-white p-3 dark:border-neutral-700 dark:bg-neutral-900">
            <p class="text-[11px] font-semibold uppercase tracking-wide text-neutral-500 dark:text-neutral-400">{{ item.label }}</p>
            <p class="mt-1 text-lg font-semibold text-neutral-900 dark:text-neutral-50">{{ item.value }}</p>
          </div>
        </div>

        <div v-if="preview.warnings.length" class="rounded-lg border border-amber-200 bg-amber-50 px-3 py-2 text-sm text-amber-800 dark:border-amber-900/60 dark:bg-amber-950/30 dark:text-amber-200">
          <p v-for="warning in preview.warnings" :key="warning">{{ warning }}</p>
        </div>

        <div v-if="blockingItems.length" class="rounded-lg border border-rose-200 bg-rose-50 px-3 py-2 text-sm text-rose-800 dark:border-rose-900/60 dark:bg-rose-950/30 dark:text-rose-200">
          <p v-for="item in blockingItems" :key="`${item.code}:${item.signal_id ?? 'none'}:${item.channel_id ?? 'none'}`">
            {{ item.message }}
          </p>
        </div>

        <div v-if="preview.skipped.length" class="rounded-lg border border-neutral-200 bg-neutral-50 px-3 py-2 text-sm text-neutral-600 dark:border-neutral-700 dark:bg-neutral-900/50 dark:text-neutral-300">
          <p class="font-semibold text-neutral-800 dark:text-neutral-100">Skipped {{ preview.skipped.length }}</p>
          <p v-for="item in preview.skipped.slice(0, 5)" :key="`${item.code}:${item.signal_id ?? 'none'}:${item.channel_id ?? 'none'}`">
            {{ item.message }}
          </p>
        </div>

        <div class="min-h-0 overflow-hidden rounded-lg border border-neutral-200 dark:border-neutral-700">
          <div class="grid grid-cols-[minmax(120px,1.2fr)_96px_minmax(120px,1fr)_minmax(120px,1fr)] border-b border-neutral-200 bg-neutral-50 text-xs font-semibold text-neutral-600 dark:border-neutral-700 dark:bg-neutral-900 dark:text-neutral-300">
            <div class="px-3 py-2">Signal</div>
            <div class="px-3 py-2">Action</div>
            <div class="px-3 py-2">Current</div>
            <div class="px-3 py-2">Proposed</div>
          </div>
          <div class="max-h-80 overflow-auto">
            <div
              v-for="change in visibleChanges"
              :key="`${change.signal_id}:${change.action}:${change.proposed_channel_id ?? 'none'}`"
              class="grid grid-cols-[minmax(120px,1.2fr)_96px_minmax(120px,1fr)_minmax(120px,1fr)] border-b border-neutral-100 text-sm last:border-b-0 dark:border-neutral-800"
            >
              <div class="min-w-0 px-3 py-2">
                <p class="truncate font-medium text-neutral-900 dark:text-neutral-50">{{ change.signal_name || change.signal_key || `Signal ${change.signal_id}` }}</p>
                <p class="truncate text-xs text-neutral-500 dark:text-neutral-400">#{{ change.signal_id }}</p>
              </div>
              <div class="px-3 py-2">
                <span :class="actionClass(change.action)">{{ actionLabel(change.action) }}</span>
              </div>
              <div class="min-w-0 truncate px-3 py-2 text-neutral-700 dark:text-neutral-200">
                {{ change.current_channel_label || "-" }}
              </div>
              <div class="min-w-0 px-3 py-2">
                <p class="truncate text-neutral-900 dark:text-neutral-50">{{ change.proposed_channel_label || "-" }}</p>
                <p v-if="change.warning" class="truncate text-xs text-amber-700 dark:text-amber-200">{{ change.warning }}</p>
              </div>
            </div>
            <div v-if="visibleChanges.length === 0" class="px-3 py-6 text-center text-sm text-neutral-500 dark:text-neutral-400">
              No changes proposed.
            </div>
          </div>
        </div>

        <p v-if="hiddenChangeCount > 0" class="text-xs text-neutral-500 dark:text-neutral-400">
          Showing first {{ visibleChanges.length }} of {{ preview.changes.length }} proposed rows.
        </p>
      </template>
    </div>

    <template #footer>
      <div class="flex w-full flex-wrap items-center justify-between gap-2">
        <p class="text-xs text-neutral-500 dark:text-neutral-400">
          {{ footerText }}
        </p>
        <div class="flex items-center gap-2">
          <UiButton type="button" variant="secondary" :disabled="applying" @click="emit('close')">Cancel</UiButton>
          <UiButton type="button" variant="primary" :disabled="!canApply || applying" @click="emit('confirm')">
            {{ applying ? "Applying..." : confirmLabel }}
          </UiButton>
        </div>
      </div>
    </template>
  </UiModal>
</template>

<script setup lang="ts">
import { computed } from "vue"

import UiButton from "@/components/ui/UiButton.vue"
import UiModal from "@/components/ui/UiModal.vue"
import type { SignalAllocationConflict, SignalAllocationPreviewChange, SignalAllocationPreviewResponse, SignalAllocationRejectedItem } from "@/types/signal"

const props = defineProps<{
  open: boolean
  title: string
  confirmLabel: string
  preview: SignalAllocationPreviewResponse | null
  loading: boolean
  applying: boolean
  error: string | null
}>()

const emit = defineEmits<{
  (event: "close"): void
  (event: "confirm"): void
}>()

const visibleChanges = computed(() => props.preview?.changes.slice(0, 80) ?? [])
const hiddenChangeCount = computed(() => Math.max(0, (props.preview?.changes.length ?? 0) - visibleChanges.value.length))
const blockingItems = computed<Array<SignalAllocationRejectedItem | SignalAllocationConflict>>(() => [
  ...(props.preview?.conflicts ?? []),
  ...(props.preview?.rejected ?? []),
])
const canApply = computed(() => (
  !props.loading
  && !props.applying
  && (props.preview?.summary.will_change ?? 0) > 0
  && blockingItems.value.length === 0
))

const summaryItems = computed(() => {
  const summary = props.preview?.summary
  return [
    { key: "requested", label: "Requested", value: summary?.requested ?? 0 },
    { key: "will_change", label: "Changes", value: summary?.will_change ?? 0 },
    { key: "assign", label: "Assign", value: summary?.assign ?? 0 },
    { key: "reassign", label: "Reassign", value: summary?.reassign ?? 0 },
    { key: "unassign", label: "Unassign", value: summary?.unassign ?? 0 },
    { key: "skipped", label: "Skipped", value: summary?.skipped ?? 0 },
  ]
})

const footerText = computed(() => {
  if (props.loading) return "Preview is loading."
  if (!props.preview) return ""
  if (blockingItems.value.length > 0) return "Resolve blocking items before applying."
  if ((props.preview.summary.will_change ?? 0) <= 0) return "No writable changes in this preview."
  return "Apply will run backend validation again."
})

function actionLabel(action: SignalAllocationPreviewChange["action"]): string {
  if (action === "assign") return "Assign"
  if (action === "reassign") return "Reassign"
  if (action === "unassign") return "Unassign"
  if (action === "noop") return "No change"
  return String(action)
}

function actionClass(action: SignalAllocationPreviewChange["action"]): string {
  const base = "inline-flex rounded-full border px-2 py-0.5 text-[11px] font-semibold"
  if (action === "assign") return `${base} border-emerald-300 bg-emerald-50 text-emerald-700 dark:border-emerald-800 dark:bg-emerald-950/40 dark:text-emerald-200`
  if (action === "reassign") return `${base} border-sky-300 bg-sky-50 text-sky-700 dark:border-sky-800 dark:bg-sky-950/40 dark:text-sky-200`
  if (action === "unassign") return `${base} border-amber-300 bg-amber-50 text-amber-800 dark:border-amber-800 dark:bg-amber-950/40 dark:text-amber-200`
  return `${base} border-neutral-300 bg-neutral-50 text-neutral-700 dark:border-neutral-700 dark:bg-neutral-900 dark:text-neutral-200`
}
</script>
