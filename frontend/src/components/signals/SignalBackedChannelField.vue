<template>
  <div class="signal-backed-field" v-bind="rootAttrs">
    <div v-if="props.showModeToggle" class="signal-backed-field__mode">
      <DirectSignalModeTabs
        :model-value="mode"
        :show-signal="canUseSignalMode"
        :disabled="props.disabled"
        @update:model-value="setMode"
      />
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
        <div
          class="signal-backed-field__signal-label"
          :class="{ 'is-scrollable': props.signalLabelScrollable }"
          :title="selectedSignalLabel"
        >
          {{ selectedSignalLabel }}
        </div>
        <div v-if="props.showSignalSubtitle && selectedSignalSubtitle" class="signal-backed-field__signal-subtitle">
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
import { computed, ref, watch, useAttrs } from "vue"

import UiButton from "@/components/ui/UiButton.vue"
import DirectSignalModeTabs from "@/components/ui/DirectSignalModeTabs.vue"
import ChannelSelect from "@/components/ui/ChannelSelect.vue"
import SignalSelectionGridModal from "@/components/signals/SignalSelectionGridModal.vue"

import type { ChannelType } from "@/types/channel"
import type { SignalAllocationRow, SignalIODirection } from "@/types/signal"
import { useSignalSheetStore } from "@/stores/signalSheetStore"
import { useToastStore } from "@/stores/toastStore"

type SelectMode = "direct" | "signal"

defineOptions({
  inheritAttrs: false,
})

const attrs = useAttrs()
const rootAttrs = computed(() => attrs)

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
  signalDisplayMode?: "default" | "source-row"
  signalDisplayDelimiter?: string
  signalLabelScrollable?: boolean
  showSignalSubtitle?: boolean
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
  signalDisplayMode: "default",
  signalDisplayDelimiter: " | ",
  signalLabelScrollable: false,
  showSignalSubtitle: true,
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

function stringifySignalSummaryValue(value: unknown): string {
  if (value === null || value === undefined) {
    return ""
  }
  if (typeof value === "string") {
    return value.trim()
  }
  if (typeof value === "number" || typeof value === "boolean") {
    return String(value)
  }
  if (Array.isArray(value)) {
    return value
      .map(item => stringifySignalSummaryValue(item))
      .filter(item => item.length > 0)
      .join(", ")
  }
  if (typeof value === "object") {
    try {
      return JSON.stringify(value)
    } catch {
      return ""
    }
  }
  return String(value)
}

function extractSourceRow(signalMetadata: unknown): Record<string, unknown> {
  if (!signalMetadata || typeof signalMetadata !== "object" || Array.isArray(signalMetadata)) {
    return {}
  }
  const row = (signalMetadata as Record<string, unknown>).row
  if (!row || typeof row !== "object" || Array.isArray(row)) {
    return {}
  }
  return row as Record<string, unknown>
}

function resolveSourceRowSummary(row: SignalAllocationRow): string {
  const sourceRow = extractSourceRow(row.signal_metadata)
  const delimiter = String(props.signalDisplayDelimiter ?? " | ") || " | "
  const segments = Object.values(sourceRow)
    .map(rawValue => stringifySignalSummaryValue(rawValue))
    .filter(segment => segment.length > 0)
  if (segments.length > 0) {
    return segments.join(delimiter)
  }
  return row.signal_name || row.signal_key
}

const selectedSignalLabel = computed(() => {
  const row = selectedSignalRow.value
  if (row) {
    if (props.signalDisplayMode === "source-row") {
      return resolveSourceRowSummary(row)
    }
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

  const excludedIds = new Set((props.excludeIds ?? []).map(id => Number(id)).filter(id => Number.isFinite(id) && id > 0))
  if (excludedIds.has(resolvedChannelId)) {
    toastStore.error("Signal resolves to a channel that is already used by another binding")
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
  display: block;
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

.signal-backed-field__signal-label.is-scrollable {
  overflow-x: auto;
  overflow-y: hidden;
  text-overflow: clip;
  scrollbar-width: none;
  -ms-overflow-style: none;
}

.signal-backed-field__signal-label.is-scrollable::-webkit-scrollbar {
  width: 0;
  height: 0;
  display: none;
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
