<template>
  <UiModal :open="open" title="Export Cable Schedule" maxWidthClass="max-w-3xl" @close="emitClose">
    <form id="signal-export-form" class="space-y-4" @submit.prevent="handleSubmit">
      <UiAlert
        type="info"
        message="Engineer note: select columns that help quickly identify connection location (panel, cabinet, bay, line, etc.)."
      />

      <div class="space-y-2">
        <p class="text-xs font-semibold uppercase tracking-wide text-neutral-500 dark:text-neutral-400">Preset (optional)</p>
        <UiAffinoListbox
          v-model="selectedPresetId"
          :options="presetListboxOptions"
          placeholder="Manual selection"
          aria-label="Export preset"
        />
        <div class="flex justify-end">
          <UiButton
            v-if="selectedPreset"
            type="button"
            variant="ghost"
            size="xs"
            @click="requestDeleteSelectedPreset"
          >
            Delete selected preset
          </UiButton>
        </div>
      </div>

      <div class="rounded-xl border border-neutral-200 bg-neutral-50 p-3 dark:border-neutral-700 dark:bg-neutral-900/40">
        <p class="text-sm font-semibold text-neutral-800 dark:text-neutral-100">Required columns (read-only)</p>
        <div class="mt-2 flex flex-wrap gap-2">
          <span
            v-for="column in requiredColumns"
            :key="column.key"
            class="rounded-md border border-neutral-300 bg-white px-2 py-1 text-xs font-medium text-neutral-700 dark:border-neutral-600 dark:bg-neutral-900 dark:text-neutral-200"
          >
            {{ column.label }}
          </span>
        </div>
      </div>

      <div class="space-y-2">
        <div class="flex items-center justify-between">
          <p class="text-sm font-semibold text-neutral-800 dark:text-neutral-100">Optional columns from grid</p>
          <div class="flex gap-2">
            <UiButton type="button" variant="ghost" size="xs" @click="selectAllOptional">Select all</UiButton>
            <UiButton type="button" variant="ghost" size="xs" @click="clearOptional">Clear</UiButton>
          </div>
        </div>

        <div v-if="optionalColumns.length" class="grid gap-2 sm:grid-cols-2 lg:grid-cols-3">
          <label
            v-for="column in optionalColumns"
            :key="column.key"
            class="flex items-center gap-2 rounded-lg border border-neutral-200 bg-white px-3 py-2 text-sm dark:border-neutral-700 dark:bg-neutral-900"
          >
            <input
              type="checkbox"
              autocomplete="off"
              class="h-4 w-4 rounded border-neutral-300 text-primary-600 focus:ring-primary-500"
              :checked="isOptionalSelected(column.key)"
              @change="toggleOptional(column.key)"
            />
            <span class="text-neutral-800 dark:text-neutral-100">{{ column.label }}</span>
          </label>
        </div>
        <p v-else class="text-sm text-neutral-500 dark:text-neutral-400">No optional columns available.</p>
      </div>

      <div>
        <label class="mb-1 block text-sm font-medium text-neutral-700 dark:text-neutral-200">Save as preset (optional)</label>
        <input
          v-model="savePresetName"
          type="text"
          autocomplete="off"
          maxlength="120"
          class="w-full rounded-lg border border-neutral-300 bg-white px-3 py-2 text-sm text-neutral-900 focus:outline-none dark:border-neutral-700 dark:bg-neutral-900 dark:text-neutral-100"
          placeholder="e.g. Cabinet wiring schedule"
        />
      </div>
    </form>

    <template #footer>
      <div class="flex w-full flex-wrap items-center justify-end gap-2">
        <UiButton type="button" variant="secondary" @click="emitClose">Cancel</UiButton>
        <UiButton type="submit" form="signal-export-form" variant="primary">Export CSV</UiButton>
      </div>
    </template>
  </UiModal>

  <ConfirmModal
    :open="deletePresetOpen"
    title="Delete preset"
    :message="deletePresetMessage"
    confirm-label="Delete"
    cancel-label="Cancel"
    @cancel="deletePresetOpen = false"
    @confirm="confirmDeleteSelectedPreset"
  />
</template>

<script setup lang="ts">
import { computed, ref, watch } from "vue"

import ConfirmModal from "@/components/ui/ConfirmModal.vue"
import UiAffinoListbox from "@/components/ui/UiAffinoListbox.vue"
import UiAlert from "@/components/ui/UiAlert.vue"
import UiButton from "@/components/ui/UiButton.vue"
import UiModal from "@/components/ui/UiModal.vue"

export type ExportColumnOption = {
  key: string
  label: string
}

type ExportPreset = {
  id: string
  name: string
  optionalColumnKeys: string[]
  createdAt: string
  updatedAt: string
}

const props = defineProps<{
  open: boolean
  workspaceId: number | null
  requiredColumns: ExportColumnOption[]
  optionalColumns: ExportColumnOption[]
}>()

const emit = defineEmits<{
  (e: "close"): void
  (e: "export", payload: { optionalColumnKeys: string[] }): void
}>()

const selectedPresetId = ref<string | null>(null)
const savePresetName = ref("")
const selectedOptionalColumnKeys = ref<string[]>([])
const deletePresetOpen = ref(false)

const presetStorageKey = computed(() => `signals:export-presets:${props.workspaceId ?? "none"}`)
const presets = ref<ExportPreset[]>([])

const presetListboxOptions = computed(() => [
  { value: null, label: "Manual selection" },
  ...presets.value.map(item => ({ value: item.id, label: item.name })),
])

const selectedPreset = computed(() => presets.value.find(item => item.id === selectedPresetId.value) ?? null)
const deletePresetMessage = computed(() => {
  const preset = selectedPreset.value
  if (!preset) return ""
  return `Preset "${preset.name}" will be deleted.`
})

function emitClose() {
  emit("close")
}

function hydratePresets() {
  if (typeof window === "undefined") return
  try {
    const raw = window.localStorage.getItem(presetStorageKey.value)
    if (!raw) {
      presets.value = []
      return
    }
    const parsed = JSON.parse(raw)
    if (!Array.isArray(parsed)) {
      presets.value = []
      return
    }
    presets.value = parsed
      .filter(item => item && typeof item === "object")
      .map((item) => ({
        id: String((item as { id?: unknown }).id ?? ""),
        name: String((item as { name?: unknown }).name ?? "").trim(),
        optionalColumnKeys: Array.isArray((item as { optionalColumnKeys?: unknown }).optionalColumnKeys)
          ? ((item as { optionalColumnKeys: unknown[] }).optionalColumnKeys.map(value => String(value)))
          : [],
        createdAt: String((item as { createdAt?: unknown }).createdAt ?? ""),
        updatedAt: String((item as { updatedAt?: unknown }).updatedAt ?? ""),
      }))
      .filter(item => item.id && item.name)
      .sort((a, b) => b.updatedAt.localeCompare(a.updatedAt))
  } catch {
    presets.value = []
  }
}

function persistPresets() {
  if (typeof window === "undefined") return
  window.localStorage.setItem(presetStorageKey.value, JSON.stringify(presets.value))
}

function normalizeOptionalSelection(keys: readonly string[]) {
  const allowed = new Set(props.optionalColumns.map(item => item.key))
  return Array.from(new Set(keys.filter(key => allowed.has(key))))
}

function selectAllOptional() {
  selectedOptionalColumnKeys.value = props.optionalColumns.map(item => item.key)
}

function clearOptional() {
  selectedOptionalColumnKeys.value = []
}

function toggleOptional(key: string) {
  const current = new Set(selectedOptionalColumnKeys.value)
  if (current.has(key)) {
    current.delete(key)
  } else {
    current.add(key)
  }
  selectedOptionalColumnKeys.value = normalizeOptionalSelection(Array.from(current))
}

function isOptionalSelected(key: string) {
  return selectedOptionalColumnKeys.value.includes(key)
}

function applySelectedPreset() {
  const preset = selectedPreset.value
  if (!preset) return
  selectedOptionalColumnKeys.value = normalizeOptionalSelection(preset.optionalColumnKeys)
}

function requestDeleteSelectedPreset() {
  if (!selectedPreset.value) return
  deletePresetOpen.value = true
}

function confirmDeleteSelectedPreset() {
  const preset = selectedPreset.value
  if (!preset) {
    deletePresetOpen.value = false
    return
  }
  presets.value = presets.value.filter(item => item.id !== preset.id)
  persistPresets()
  selectedPresetId.value = null
  deletePresetOpen.value = false
}

function upsertPreset(name: string, optionalColumnKeys: string[]) {
  const normalizedName = name.trim()
  if (!normalizedName) return

  const nowIso = new Date().toISOString()
  const existing = presets.value.find(item => item.name.toLowerCase() === normalizedName.toLowerCase())
  if (existing) {
    existing.optionalColumnKeys = [...optionalColumnKeys]
    existing.updatedAt = nowIso
  } else {
    presets.value.unshift({
      id: `${Date.now()}-${Math.random().toString(36).slice(2, 8)}`,
      name: normalizedName,
      optionalColumnKeys: [...optionalColumnKeys],
      createdAt: nowIso,
      updatedAt: nowIso,
    })
  }
  presets.value.sort((a, b) => b.updatedAt.localeCompare(a.updatedAt))
  persistPresets()
}

function handleSubmit() {
  const optionalColumnKeys = normalizeOptionalSelection(selectedOptionalColumnKeys.value)
  if (savePresetName.value.trim()) {
    upsertPreset(savePresetName.value, optionalColumnKeys)
  }
  emit("export", { optionalColumnKeys })
  emitClose()
}

watch(() => props.open, (opened) => {
  if (!opened) return
  hydratePresets()
  selectedPresetId.value = null
  savePresetName.value = ""
  selectedOptionalColumnKeys.value = []
})

watch(() => props.optionalColumns, () => {
  selectedOptionalColumnKeys.value = normalizeOptionalSelection(selectedOptionalColumnKeys.value)
}, { deep: true })

watch(() => selectedPresetId.value, () => {
  applySelectedPreset()
})

watch(() => props.workspaceId, () => {
  if (!props.open) return
  hydratePresets()
  selectedPresetId.value = null
  selectedOptionalColumnKeys.value = []
})
</script>
