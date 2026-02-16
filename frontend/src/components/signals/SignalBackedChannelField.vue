<template>
  <div class="signal-backed-field">
    <div v-if="props.showModeToggle" class="signal-backed-field__mode">
      <button
        type="button"
        class="signal-backed-field__mode-btn"
        :class="{ 'is-active': mode === 'direct' }"
        :disabled="props.disabled"
        @click="setMode('direct')"
      >
        Direct
      </button>
      <button
        v-if="canUseSignalMode"
        type="button"
        class="signal-backed-field__mode-btn"
        :class="{ 'is-active': mode === 'signal' }"
        :disabled="props.disabled"
        @click="setMode('signal')"
      >
        By Signal
      </button>
    </div>

    <div v-if="mode === 'direct' || !canUseSignalMode" class="signal-backed-field__panel">
      <ChannelSelect
        :model-value="props.channelId"
        :channel-type="props.channelType"
        :exclude-ids="props.excludeIds"
        :name="props.name"
        :disabled="props.disabled"
        @update:modelValue="handleDirectChannelChange"
      />
    </div>

    <div v-else class="signal-backed-field__panel signal-backed-field__panel--signal">
      <div class="signal-backed-field__signal-meta">
        <div class="signal-backed-field__signal-label">
          {{ selectedSignalLabel }}
        </div>
        <div v-if="selectedSignalSubtitle" class="signal-backed-field__signal-subtitle">
          {{ selectedSignalSubtitle }}
        </div>
      </div>
      <div class="signal-backed-field__actions">
        <UiButton
          variant="secondary"
          size="xs"
          :disabled="props.disabled || !canUseSignalMode"
          @click="signalModalOpen = true"
        >
          Pick signal
        </UiButton>
        <UiButton
          v-if="props.showSignalClear"
          variant="ghost"
          size="xs"
          :disabled="props.disabled || !canClearSignal"
          @click="clearSignalSelection"
        >
          Clear
        </UiButton>
      </div>
    </div>
  </div>

  <SignalSelectionGridModal
    :open="signalModalOpen"
    :title="signalPickerTitleComputed"
    :allowed-directions="allowedDirections"
    :multiple="false"
    :min-selected="1"
    :confirm-label="'Select signal'"
    :table-id="props.tableId"
    @close="signalModalOpen = false"
    @confirm="handleSignalConfirm"
  />
</template>

<script setup lang="ts">
import { computed, ref, watch } from "vue"

import UiButton from "@/components/ui/UiButton.vue"
import ChannelSelect from "@/components/ui/ChannelSelect.vue"
import SignalSelectionGridModal from "@/components/signals/SignalSelectionGridModal.vue"

import type { ChannelType } from "@/types/channel"
import type { SignalAllocationRow, SignalIODirection } from "@/types/signal"
import { useSignalSheetStore } from "@/stores/signalSheetStore"
import { useToastStore } from "@/stores/toastStore"

type SelectMode = "direct" | "signal"

const props = withDefaults(defineProps<{
  channelId: number | null
  channelType: ChannelType
  signalId?: number | null
  signalKey?: string | null
  mode?: SelectMode
  disabled?: boolean
  excludeIds?: number[]
  name?: string
  signalPickerTitle?: string
  tableId?: string
  allowSignalMode?: boolean
  showModeToggle?: boolean
  showSignalClear?: boolean
  emptySignalSubtitle?: string
}>(), {
  signalId: null,
  signalKey: null,
  disabled: false,
  excludeIds: () => [],
  name: undefined,
  signalPickerTitle: "Select signal",
  tableId: "signal-backed-channel-grid",
  allowSignalMode: true,
  showModeToggle: true,
  showSignalClear: true,
  emptySignalSubtitle: "Choose a signal, then allocation to hardware is ensured automatically.",
})

const emit = defineEmits<{
  (e: "update:channelId", value: number | null): void
  (e: "update:signal", value: { signalId: number | null; signalKey: string | null }): void
  (e: "update:mode", value: SelectMode): void
}>()

const signalSheetStore = useSignalSheetStore()
const toastStore = useToastStore()
const canUseSignalMode = computed(() => props.allowSignalMode && signalSheetStore.hasSheet)

function resolveInitialMode(): SelectMode {
  if (!canUseSignalMode.value) {
    return "direct"
  }
  if (props.mode === "signal" || props.mode === "direct") {
    return props.mode
  }
  if (props.signalId !== null || Boolean(props.signalKey)) {
    return "signal"
  }
  return signalSheetStore.hasSheet ? "signal" : "direct"
}

const mode = ref<SelectMode>(resolveInitialMode())
const signalModalOpen = ref(false)

const allowedDirections = computed<SignalIODirection[]>(() => {
  switch (props.channelType) {
    case "do":
      return ["DI"]
    case "di":
      return ["DO"]
    case "ao":
      return ["AI"]
    default:
      return []
  }
})

const signalPickerTitleComputed = computed(() => props.signalPickerTitle)

const selectedSignalRow = computed(() => {
  if (Number.isFinite(props.signalId as number)) {
    const byId = signalSheetStore.allocationRows.find(row => row.signal_id === Number(props.signalId))
    if (byId) return byId
  }
  const normalizedSignalKey = String(props.signalKey ?? "").trim()
  if (normalizedSignalKey) {
    const byKey = signalSheetStore.allocationRows.find(row => row.signal_key === normalizedSignalKey)
    if (byKey) return byKey
  }
  if (!Number.isFinite(props.channelId as number)) {
    return null
  }
  return signalSheetStore.allocationRows.find((row) => {
    const rowChannelId = Number.isFinite(row.channel_id as number) ? Number(row.channel_id) : null
    if (rowChannelId !== Number(props.channelId)) {
      return false
    }
    const rowChannelType = normalizeChannelType(row.channel_type)
    return rowChannelType === props.channelType
  }) ?? null
})

const canClearSignal = computed(() => (
  Number.isFinite(props.channelId as number)
  || Number.isFinite(props.signalId as number)
  || Boolean(props.signalKey)
))

const selectedSignalLabel = computed(() => {
  const row = selectedSignalRow.value
  if (row) {
    return row.signal_name || row.signal_key
  }
  if (props.signalKey) {
    return props.signalKey
  }
  return "No signal selected"
})

const selectedSignalSubtitle = computed(() => {
  const row = selectedSignalRow.value
  if (!row) {
    return props.emptySignalSubtitle
  }
  if (Number.isFinite(row.channel_index as number)) {
    const suffix = `ch${Number(row.channel_index) + 1}`
    const unitChannel = row.unit_id?.trim() ? `${row.unit_id}/${suffix}` : suffix
    return `${row.signal_direction} → ${unitChannel}`
  }
  return `${row.signal_direction} → unallocated`
})

watch(
  () => props.mode,
  (nextMode) => {
    if (nextMode === "signal" && !canUseSignalMode.value) {
      mode.value = "direct"
      return
    }
    if (nextMode && nextMode !== mode.value) {
      mode.value = nextMode
    }
  },
)

watch(
  canUseSignalMode,
  (enabled) => {
    if (enabled || mode.value !== "signal") return
    mode.value = "direct"
    emit("update:mode", "direct")
  },
)

watch(
  () => [props.signalId, props.signalKey] as const,
  ([nextSignalId, nextSignalKey]) => {
    if (mode.value === "direct") return
    if (nextSignalId !== null || Boolean(nextSignalKey)) return
    mode.value = props.mode ?? (canUseSignalMode.value ? "signal" : "direct")
  },
)

watch(
  () => mode.value,
  (nextMode) => {
    if (!canUseSignalMode.value) return
    if (nextMode !== "signal") return
    if (signalSheetStore.allocationRows.length > 0) return
    void signalSheetStore.refreshAllocations().catch(() => undefined)
  },
  { immediate: true },
)

function setMode(nextMode: SelectMode) {
  if (nextMode === "signal" && !canUseSignalMode.value) return
  if (props.disabled || mode.value === nextMode) return
  mode.value = nextMode
  emit("update:mode", nextMode)
  if (nextMode === "direct") {
    emit("update:signal", { signalId: null, signalKey: null })
  }
}

function handleDirectChannelChange(nextChannelId: number | null) {
  emit("update:channelId", nextChannelId)
  emit("update:signal", { signalId: null, signalKey: null })
}

function clearSignalSelection() {
  emit("update:signal", { signalId: null, signalKey: null })
  emit("update:channelId", null)
}

function normalizeChannelType(value: unknown): ChannelType | null {
  const normalized = String(value ?? "").trim().toLowerCase()
  if (normalized.startsWith("di")) return "di"
  if (normalized.startsWith("do")) return "do"
  if (normalized.startsWith("ao")) return "ao"
  return null
}

async function handleSignalConfirm(rows: SignalAllocationRow[]) {
  signalModalOpen.value = false
  const picked = rows[0]
  if (!picked) return

  try {
    await signalSheetStore.ensureAllocated([picked.signal_id], { preferOnline: true })
  } catch (error) {
    toastStore.error(error instanceof Error ? error.message : "Failed to ensure signal allocation")
    return
  }

  const resolved = signalSheetStore.allocationRows.find(row => row.signal_id === picked.signal_id) ?? picked
  const resolvedChannelId = Number.isFinite(resolved.channel_id as number) ? Number(resolved.channel_id) : null
  if (resolvedChannelId === null) {
    toastStore.info("No free compatible channel available for this signal")
    return
  }

  const resolvedChannelType = normalizeChannelType(resolved.channel_type)
  if (resolvedChannelType !== props.channelType) {
    toastStore.error("Signal is incompatible with the selected channel type")
    return
  }

  mode.value = "signal"
  emit("update:mode", "signal")
  emit("update:signal", { signalId: resolved.signal_id, signalKey: resolved.signal_key })
  emit("update:channelId", resolvedChannelId)
}
</script>

<style scoped>
.signal-backed-field {
  display: flex;
  flex-direction: column;
  gap: 0.5rem;
}

.signal-backed-field__mode {
  display: inline-flex;
  align-items: center;
  gap: 0.25rem;
  padding: 0.125rem;
  border-radius: 0.5rem;
  border: 1px solid rgb(212 212 212);
  background: rgb(250 250 250);
}

.dark .signal-backed-field__mode {
  border-color: rgb(64 64 64);
  background: rgb(23 23 23);
}

.signal-backed-field__mode-btn {
  border: 0;
  background: transparent;
  color: rgb(82 82 82);
  border-radius: 0.375rem;
  padding: 0.35rem 0.75rem;
  font-size: 0.75rem;
  line-height: 1;
  font-weight: 700;
  border: 1px solid transparent;
  transition: background-color 120ms ease, border-color 120ms ease, color 120ms ease;
}

.signal-backed-field__mode-btn.is-active {
  background: rgb(23 23 23);
  border-color: rgb(23 23 23);
  color: rgb(255 255 255);
}

.dark .signal-backed-field__mode-btn {
  color: rgb(212 212 212);
  border-color: rgb(82 82 82);
  background: rgb(24 24 27);
}

.dark .signal-backed-field__mode-btn.is-active {
  background: rgb(245 245 245);
  border-color: rgb(245 245 245);
  color: rgb(15 23 42);
}

.signal-backed-field__panel {
  min-width: 0;
}

.signal-backed-field__panel--signal {
  display: flex;
  flex-wrap: wrap;
  align-items: flex-start;
  justify-content: space-between;
  gap: 0.5rem;
  border: 1px solid rgb(229 229 229);
  border-radius: 0.5rem;
  padding: 0.5rem;
  background: rgb(250 250 250);
}

.dark .signal-backed-field__panel--signal {
  border-color: rgb(64 64 64);
  background: rgb(15 23 42 / 0.35);
}

.signal-backed-field__signal-meta {
  min-width: 0;
  flex: 1;
}

.signal-backed-field__signal-label {
  font-size: 0.8rem;
  font-weight: 700;
  color: rgb(23 23 23);
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

.dark .signal-backed-field__signal-label {
  color: rgb(245 245 245);
}

.signal-backed-field__signal-subtitle {
  margin-top: 0.1rem;
  font-size: 0.68rem;
  color: rgb(115 115 115);
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

.dark .signal-backed-field__signal-subtitle {
  color: rgb(163 163 163);
}

.signal-backed-field__actions {
  display: flex;
  align-items: center;
  gap: 0.35rem;
}
</style>
