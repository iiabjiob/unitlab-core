<template>
  <div class="h-full flex flex-col gap-4">
    <div class="space-y-3">
      <UiButton variant="primary" size="sm" full :disabled="workspaceMissing" @click="emit('import')">
        📥 Import Signal List
      </UiButton>
      <UiButton variant="ghost" size="sm" full :disabled="workspaceMissing || loading" @click="emit('refresh')">
        Refresh
      </UiButton>
      <p
        v-if="workspaceMissing"
        class="text-[11px] uppercase tracking-[0.3em] text-neutral-500 dark:text-neutral-400"
      >
        Choose a workspace to view signal sheet
      </p>
    </div>

    <section class="rounded-2xl border border-neutral-200 bg-neutral-50 px-3 py-3 text-xs dark:border-neutral-700 dark:bg-neutral-900/40">
      <p class="text-[11px] uppercase tracking-[0.2em] text-neutral-500 dark:text-neutral-400">Active Signal Sheet</p>
      <div v-if="!sheet || sheet.signals_count === 0" class="mt-2 text-neutral-500 dark:text-neutral-400">
        No sheet imported yet.
      </div>
      <div v-else class="mt-2 space-y-1 text-neutral-700 dark:text-neutral-200">
        <p class="font-medium">{{ sheet.source_filename ?? "Imported signal list" }}</p>
        <p>{{ sheet.signals_count }} signals · {{ sheet.allocated_count }} allocated</p>
        <p>{{ sheet.rows_count }} source rows · updated {{ formatDate(sheet.updated_at) }}</p>
      </div>
    </section>

    <section class="min-h-0 flex-1 overflow-hidden">
      <div class="mb-2 flex items-center justify-between">
        <p class="text-xs font-semibold uppercase tracking-[0.2em] text-neutral-500 dark:text-neutral-400">Import Presets</p>
        <span class="text-xs text-neutral-500 dark:text-neutral-400">{{ presets.length }}</span>
      </div>

      <UiSidebarListbox
        :items="presets"
        :active-id="null"
        aria-label="Signal import presets"
        class="h-full min-h-0"
      >
        <template #item="{ item: preset, isCursor }">
          <button
            class="w-full rounded-xl px-3 py-2 text-left transition"
            :class="isCursor ? 'bg-neutral-100 dark:bg-neutral-800' : 'hover:bg-neutral-100/70 dark:hover:bg-neutral-800/80'"
            type="button"
            @click="noop"
          >
            <p class="truncate text-sm font-medium text-neutral-800 dark:text-neutral-100">{{ preset.name }}</p>
            <p class="mt-1 text-xs text-neutral-500 dark:text-neutral-400">Updated {{ formatDate(preset.updated_at) }}</p>
            <div class="mt-2 flex justify-end">
              <UiButton variant="ghost" size="xs" @click.stop="requestDeletePreset(preset)">Delete</UiButton>
            </div>
          </button>
        </template>
        <template #empty>
          <div class="rounded-2xl border border-dashed border-neutral-300/70 px-4 py-6 text-center text-xs text-neutral-500 dark:border-neutral-700 dark:text-neutral-400">
            No saved presets
          </div>
        </template>
      </UiSidebarListbox>
    </section>

    <ConfirmModal
      :open="deleteOpen"
      title="Delete preset"
      :message="deleteMessage"
      confirm-label="Delete"
      cancel-label="Cancel"
      @cancel="deleteOpen = false"
      @confirm="confirmDeletePreset"
    />
  </div>
</template>

<script setup lang="ts">
import { computed, ref } from "vue"

import ConfirmModal from "@/components/ui/ConfirmModal.vue"
import UiButton from "@/components/ui/UiButton.vue"
import UiSidebarListbox from "@/components/ui/UiSidebarListbox.vue"
import type { SignalSheet, SignalSheetPreset } from "@/types/signal"
import { useSignalSheetStore } from "@/stores/signalSheetStore"
import { useToastStore } from "@/stores/toastStore"
import { useWorkspaceStore } from "@/stores/workspaceStore"
import { formatDate } from "@/utils/datetime"

const props = defineProps<{
  sheet: SignalSheet | null
  presets: SignalSheetPreset[]
  loading?: boolean
}>()

const emit = defineEmits<{ (e: "import"): void; (e: "refresh"): void }>()

const workspaceStore = useWorkspaceStore()
const signalSheetStore = useSignalSheetStore()
const toastStore = useToastStore()

const deleteOpen = ref(false)
const deletingPreset = ref<SignalSheetPreset | null>(null)

const workspaceMissing = computed(() => !workspaceStore.activeWorkspaceId)
const loading = computed(() => props.loading ?? false)
const deleteMessage = computed(() => {
  const preset = deletingPreset.value
  if (!preset) return ""
  return `Preset "${preset.name}" will be deleted.`
})

function noop() {
  return
}

function requestDeletePreset(preset: SignalSheetPreset) {
  deletingPreset.value = preset
  deleteOpen.value = true
}

async function confirmDeletePreset() {
  const preset = deletingPreset.value
  if (!preset) return
  try {
    await signalSheetStore.deletePreset(preset.id)
    deleteOpen.value = false
    deletingPreset.value = null
    toastStore.success("Preset deleted")
  } catch (err) {
    toastStore.error(err instanceof Error ? err.message : String(err))
  }
}
</script>
