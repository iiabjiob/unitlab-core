<template>
  <UiModal :open="open" title="Export Cable Schedule" max-width="3xl" @close="emitClose">
    <form id="signal-export-form" class="signal-export-modal__form" @submit.prevent="handleSubmit">
      <UiAlert
        type="info"
        message="Engineer note: select columns that help quickly identify connection location (panel, cabinet, bay, line, etc.)."
      />

      <div class="signal-export-modal__field-stack">
        <p class="signal-export-modal__eyebrow">Preset (optional)</p>
        <UiAffinoListbox
          v-model="selectedPresetId"
          :options="presetListboxOptions"
          placeholder="Manual selection"
          aria-label="Export preset"
        />
        <div class="signal-export-modal__inline-actions">
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

      <div class="signal-export-modal__card">
        <p class="signal-export-modal__section-title">Required columns (read-only)</p>
        <div class="signal-export-modal__chip-list">
          <span
            v-for="column in requiredColumns"
            :key="column.key"
            class="signal-export-modal__chip"
          >
            {{ column.label }}
          </span>
        </div>
      </div>

      <div class="signal-export-modal__field-stack">
        <div class="signal-export-modal__section-heading">
          <p class="signal-export-modal__section-title">Optional columns from grid</p>
          <div class="signal-export-modal__button-pair">
            <UiButton type="button" variant="ghost" size="xs" @click="selectAllOptional">Select all</UiButton>
            <UiButton type="button" variant="ghost" size="xs" @click="clearOptional">Clear</UiButton>
          </div>
        </div>

        <div v-if="optionalColumns.length" class="signal-export-modal__option-list">
          <label
            v-for="column in optionalColumns"
            :key="column.key"
            class="signal-export-modal__option"
          >
            <input
              type="checkbox"
              autocomplete="off"
              class="signal-export-modal__checkbox"
              :checked="isOptionalSelected(column.key)"
              @change="toggleOptional(column.key)"
            />
            <span class="signal-export-modal__option-label">{{ column.label }}</span>
          </label>
        </div>
        <p v-else class="signal-export-modal__muted">No optional columns available.</p>
      </div>

      <div class="signal-export-modal__field">
        <label class="signal-export-modal__label">Save as preset (optional)</label>
        <input
          v-model="savePresetName"
          type="text"
          autocomplete="off"
          maxlength="120"
          class="signal-export-modal__input"
          placeholder="e.g. Cabinet wiring schedule"
        />
      </div>
    </form>

    <template #footer>
      <div class="signal-export-modal__footer">
        <UiButton type="button" variant="secondary" @click="emitClose">Cancel</UiButton>
        <UiButton type="submit" form="signal-export-form" variant="primary">📤 Export CSV</UiButton>
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
import { localSettingsKeys, readLocalSetting, writeLocalSetting } from "@/services/localSettingsStorage"

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

const presetStorageKey = computed(() => localSettingsKeys.signalExportPresets(props.workspaceId))
const legacyPresetStorageKey = computed(() => `signals:export-presets:${props.workspaceId ?? "none"}`)
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
  presets.value = readLocalSetting<ExportPreset[]>(presetStorageKey.value, [], {
    legacyKeys: [legacyPresetStorageKey.value],
    validate: normalizeExportPresets,
  })
}

function persistPresets() {
  writeLocalSetting(presetStorageKey.value, presets.value, {
    legacyKeys: [legacyPresetStorageKey.value],
  })
}

function normalizeExportPresets(value: unknown): ExportPreset[] | null {
  if (!Array.isArray(value)) {
    return null
  }

  return value
    .filter(item => item && typeof item === "object")
    .map((item) => ({
      id: String((item as { id?: unknown }).id ?? ""),
      name: String((item as { name?: unknown }).name ?? "").trim(),
      optionalColumnKeys: Array.isArray((item as { optionalColumnKeys?: unknown }).optionalColumnKeys)
        ? ((item as { optionalColumnKeys: unknown[] }).optionalColumnKeys.map(columnKey => String(columnKey)))
        : [],
      createdAt: String((item as { createdAt?: unknown }).createdAt ?? ""),
      updatedAt: String((item as { updatedAt?: unknown }).updatedAt ?? ""),
    }))
    .filter(item => item.id && item.name)
    .sort((a, b) => b.updatedAt.localeCompare(a.updatedAt))
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

<style scoped>
.signal-export-modal__form {
  display: flex;
  flex-direction: column;
  gap: 1rem;
}

.signal-export-modal__field-stack {
  display: flex;
  flex-direction: column;
  gap: 0.5rem;
}

.signal-export-modal__eyebrow {
  color: var(--color-neutral-500);
  font-size: var(--text-xs);
  font-weight: 600;
  letter-spacing: 0.025em;
  text-transform: uppercase;
}

.signal-export-modal__inline-actions {
  display: flex;
  justify-content: flex-end;
}

.signal-export-modal__card {
  background: var(--color-neutral-50);
  border: 1px solid var(--color-neutral-200);
  border-radius: 0.75rem;
  padding: 0.75rem;
}

.signal-export-modal__section-title {
  color: var(--color-neutral-800);
  font-size: var(--text-sm);
  font-weight: 600;
}

.signal-export-modal__chip-list {
  display: flex;
  flex-wrap: wrap;
  gap: 0.5rem;
  margin-top: 0.5rem;
}

.signal-export-modal__chip {
  background: var(--color-white);
  border: 1px solid var(--color-neutral-300);
  border-radius: var(--radius-md);
  color: var(--color-neutral-700);
  font-size: var(--text-xs);
  font-weight: 500;
  padding: 0.25rem 0.5rem;
}

.signal-export-modal__section-heading {
  align-items: center;
  display: flex;
  justify-content: space-between;
}

.signal-export-modal__button-pair {
  display: flex;
  gap: 0.5rem;
}

.signal-export-modal__option-list {
  display: grid;
  gap: 0.5rem;
}

.signal-export-modal__option {
  align-items: center;
  background: var(--color-white);
  border: 1px solid var(--color-neutral-200);
  border-radius: var(--radius-lg);
  display: flex;
  gap: 0.5rem;
  padding: 0.5rem 0.75rem;
}

.signal-export-modal__checkbox {
  accent-color: var(--color-blue-600);
  border: 1px solid var(--color-neutral-300);
  border-radius: var(--radius-sm);
  height: 1rem;
  width: 1rem;
}

.signal-export-modal__checkbox:focus {
  outline: 2px solid var(--color-blue-500);
  outline-offset: 2px;
}

.signal-export-modal__option-label {
  color: var(--color-neutral-800);
  font-size: var(--text-sm);
}

.signal-export-modal__muted {
  color: var(--color-neutral-500);
  font-size: var(--text-sm);
}

.signal-export-modal__field {
  display: block;
}

.signal-export-modal__label {
  color: var(--color-neutral-700);
  display: block;
  font-size: var(--text-sm);
  font-weight: 500;
  margin-bottom: 0.25rem;
}

.signal-export-modal__input {
  background: var(--color-white);
  border: 1px solid var(--color-neutral-300);
  border-radius: var(--radius-lg);
  color: var(--color-neutral-900);
  font-size: var(--text-sm);
  padding: 0.5rem 0.75rem;
  width: 100%;
}

.signal-export-modal__input:focus {
  outline: 2px solid var(--color-blue-500);
  outline-offset: 1px;
}

.signal-export-modal__footer {
  align-items: center;
  display: flex;
  flex-wrap: wrap;
  gap: 0.5rem;
  justify-content: flex-end;
  width: 100%;
}

:global(.dark .signal-export-modal__eyebrow),
:global(.dark .signal-export-modal__muted) {
  color: var(--color-neutral-400);
}

:global(.dark .signal-export-modal__card) {
  background: color-mix(in srgb, var(--color-neutral-900) 40%, transparent);
  border-color: var(--color-neutral-700);
}

:global(.dark .signal-export-modal__section-title),
:global(.dark .signal-export-modal__option-label),
:global(.dark .signal-export-modal__input) {
  color: var(--color-neutral-100);
}

:global(.dark .signal-export-modal__chip) {
  background: var(--color-neutral-900);
  border-color: var(--color-neutral-600);
  color: var(--color-neutral-200);
}

:global(.dark .signal-export-modal__option) {
  background: var(--color-neutral-900);
  border-color: var(--color-neutral-700);
}

:global(.dark .signal-export-modal__label) {
  color: var(--color-neutral-200);
}

:global(.dark .signal-export-modal__input) {
  background: var(--color-neutral-900);
  border-color: var(--color-neutral-700);
}

@media (min-width: 640px) {
  .signal-export-modal__option-list {
    grid-template-columns: repeat(2, minmax(0, 1fr));
  }
}

@media (min-width: 1024px) {
  .signal-export-modal__option-list {
    grid-template-columns: repeat(3, minmax(0, 1fr));
  }
}
</style>
