<script setup lang="ts">
import { computed, ref, watch } from "vue"

import type { Switchgear } from "@/types/switchgear"
import { CHANNEL_TYPES, type ChannelType } from "@/types/channel"
import type { SignalAllocationRow, SignalIODirection } from "@/types/signal"
import SignalBackedChannelField from "@/components/signals/SignalBackedChannelField.vue"
import SignalSelectionGridModal from "@/components/signals/SignalSelectionGridModal.vue"
import DirectSignalModeTabs from "@/components/ui/DirectSignalModeTabs.vue"
import UiButton from "@/components/ui/UiButton.vue"
import UiAlert from "@/components/ui/UiAlert.vue"
import InlineInfoTooltip from "@/components/ui/InlineInfoTooltip.vue"
import { useSwitchgearStore } from "@/stores/switchgearStore"
import { useToastStore } from "@/stores/toastStore"
import { useSignalSheetStore } from "@/stores/signalSheetStore"
import { useWorkspaceStore } from "@/stores/workspaceStore"
import { runStoreBootstrap } from "@/composables/useStoreBootstrap"

const props = defineProps<{
  switchgear: Switchgear
}>()

const emit = defineEmits<{
  (e: "close"): void
}>()

const store = useSwitchgearStore()
const toastStore = useToastStore()
const signalSheetStore = useSignalSheetStore()
const workspaceStore = useWorkspaceStore()

type BindingRoleKey = "do_open" | "do_closed" | "di_open" | "di_close"
type BindingMode = "direct" | "signal"

type EditableBinding = {
  id: number
  role: BindingRoleKey
  channel_id: number | null
  delay_ms: number
}

const ROLE_ORDER: BindingRoleKey[] = ["do_open", "do_closed", "di_open", "di_close"]
const DO_ROLES: BindingRoleKey[] = ["do_open", "do_closed"]
const DI_ROLES: BindingRoleKey[] = ["di_open", "di_close"]

const ROLE_META: Record<BindingRoleKey, {
  label: string
  channelType: ChannelType
  supportsDelay: boolean
}> = {
  do_open: { label: "OPEN position", channelType: CHANNEL_TYPES.DO, supportsDelay: false },
  do_closed: { label: "CLOSED position", channelType: CHANNEL_TYPES.DO, supportsDelay: false },
  di_open: { label: "OPEN command from BCU", channelType: CHANNEL_TYPES.DI, supportsDelay: true },
  di_close: { label: "CLOSE command from BCU", channelType: CHANNEL_TYPES.DI, supportsDelay: true },
}

const bindingsDraft = ref<EditableBinding[]>([])
const signalSelectionByRole = ref<Record<BindingRoleKey, { signalId: number | null; signalKey: string | null }>>({
  do_open: { signalId: null, signalKey: null },
  do_closed: { signalId: null, signalKey: null },
  di_open: { signalId: null, signalKey: null },
  di_close: { signalId: null, signalKey: null },
})

const applyingSignalSelection = ref(false)
const signalModalOpen = ref(false)
const selectedSignalRows = ref<SignalAllocationRow[]>([])
const error = ref<string | null>(null)
const bindingMode = ref<BindingMode>("direct")

const signalModeAvailable = computed(() => {
  const workspaceId = workspaceStore.activeWorkspaceId
  const sheet = signalSheetStore.sheet
  if (!workspaceId || !sheet) return false
  return sheet.workspace_id === workspaceId && sheet.signals_count > 0
})

const effectiveSelectedSignalRows = computed(() => {
  if (selectedSignalRows.value.length > 0) {
    return selectedSignalRows.value
  }

  const rows: SignalAllocationRow[] = []
  const seen = new Set<number>()
  for (const role of ROLE_ORDER) {
    const signalId = Number(signalSelectionByRole.value[role]?.signalId)
    if (!Number.isFinite(signalId) || signalId <= 0 || seen.has(signalId)) {
      continue
    }
    const row = signalSheetStore.allocationRows.find(item => item.signal_id === signalId)
    if (!row) {
      continue
    }
    seen.add(signalId)
    rows.push(row)
  }
  return rows
})

const selectedDiSignalRows = computed(() => effectiveSelectedSignalRows.value.filter(row => row.signal_direction === "DI"))
const selectedDoSignalRows = computed(() => effectiveSelectedSignalRows.value.filter(row => row.signal_direction === "DO"))

const signalSelectionSummary = computed(() => {
  if (!effectiveSelectedSignalRows.value.length) {
    return "No signals selected"
  }
  return `${effectiveSelectedSignalRows.value.length} selected · DI ${selectedDiSignalRows.value.length} · DO ${selectedDoSignalRows.value.length}`
})

function normalizeBindings(source: ReadonlyArray<Switchgear["bindings"][number]> | null | undefined): EditableBinding[] {
  return ROLE_ORDER.map((role) => {
    const current = source?.find(binding => binding.role === role) ?? null
    return {
      id: Number(current?.id) || 0,
      role,
      channel_id: Number.isFinite(current?.channel_id as number) ? Number(current?.channel_id) : null,
      delay_ms: Number.isFinite(current?.delay_ms as number) ? Math.max(0, Math.round(Number(current?.delay_ms))) : 0,
    }
  })
}

function normalizeChannelType(value: unknown): ChannelType | null {
  const normalized = String(value ?? "").trim().toLowerCase()
  if (normalized.startsWith("di")) return CHANNEL_TYPES.DI
  if (normalized.startsWith("do")) return CHANNEL_TYPES.DO
  if (normalized.startsWith("ao")) return CHANNEL_TYPES.AO
  return null
}

function syncSignalSelectionsFromBindings(bindings: EditableBinding[]) {
  const next: Record<BindingRoleKey, { signalId: number | null; signalKey: string | null }> = {
    do_open: { signalId: null, signalKey: null },
    do_closed: { signalId: null, signalKey: null },
    di_open: { signalId: null, signalKey: null },
    di_close: { signalId: null, signalKey: null },
  }

  for (const role of ROLE_ORDER) {
    const channelId = bindings.find(item => item.role === role)?.channel_id ?? null
    if (!Number.isFinite(channelId as number)) continue
    const row = signalSheetStore.allocationRows.find((item) => {
      const rowChannelId = Number(item.channel_id)
      if (!Number.isFinite(rowChannelId) || rowChannelId <= 0) return false
      if (rowChannelId !== Number(channelId)) return false
      const rowType = normalizeChannelType(item.channel_type)
      return rowType === ROLE_META[role].channelType
    })
    if (!row) continue
    next[role] = {
      signalId: Number.isFinite(row.signal_id as number) ? Number(row.signal_id) : null,
      signalKey: row.signal_key ?? null,
    }
  }

  signalSelectionByRole.value = next
}

function roleBinding(role: BindingRoleKey) {
  return bindingsDraft.value.find(binding => binding.role === role) ?? null
}

function channelValue(role: BindingRoleKey): number | null {
  return roleBinding(role)?.channel_id ?? null
}

function delayFor(role: BindingRoleKey): number {
  return roleBinding(role)?.delay_ms ?? 0
}

function patchRole(role: BindingRoleKey, patch: Partial<EditableBinding>) {
  bindingsDraft.value = bindingsDraft.value.map((binding) => (
    binding.role === role ? { ...binding, ...patch } : binding
  ))
}

function setMode(mode: BindingMode) {
  if (mode === "signal" && !signalModeAvailable.value) return
  bindingMode.value = mode
  error.value = null
}

function handleChannelChange(role: BindingRoleKey, channelId: number | null) {
  patchRole(role, { channel_id: channelId })
  signalSelectionByRole.value[role] = { signalId: null, signalKey: null }
  void persistBindingsDraft()
}

function handleSignalChange(role: BindingRoleKey, payload: { signalId: number | null; signalKey: string | null }) {
  signalSelectionByRole.value[role] = payload
}

function handleDelayChange(role: BindingRoleKey, value: number) {
  const safeValue = Number.isFinite(value) ? Math.max(0, Math.round(value)) : 0
  patchRole(role, { delay_ms: safeValue })
  void persistBindingsDraft()
}

function excludeIdsForRole(role: BindingRoleKey): number[] {
  return bindingsDraft.value
    .filter(binding => binding.role !== role)
    .map(binding => Number(binding.channel_id))
    .filter(channelId => Number.isFinite(channelId) && channelId > 0)
}

async function updateBindings(nextBindings: EditableBinding[]) {
  const normalized = normalizeBindings(nextBindings)
  bindingsDraft.value = normalized

  const payload = normalized.map(binding => ({
    role: binding.role,
    channel_id: binding.channel_id,
    delay_ms: binding.delay_ms,
  }))

  const updated = await store.updateField(props.switchgear.id, { bindings: payload })
  const next = normalizeBindings(updated.bindings)
  bindingsDraft.value = next
  syncSignalSelectionsFromBindings(next)
}

function validateNoDuplicateChannels(bindings: EditableBinding[]) {
  const seen = new Set<number>()
  for (const binding of bindings) {
    const channelId = Number(binding.channel_id)
    if (!Number.isFinite(channelId) || channelId <= 0) continue
    if (seen.has(channelId)) {
      throw new Error("Each role must use a unique channel")
    }
    seen.add(channelId)
  }
}

async function persistBindingsDraft() {
  error.value = null
  try {
    validateNoDuplicateChannels(bindingsDraft.value)
    await updateBindings(bindingsDraft.value)
    syncSignalSelectionsFromBindings(bindingsDraft.value)
    return
  } catch (err) {
    bindingsDraft.value = normalizeBindings(props.switchgear.bindings)
    syncSignalSelectionsFromBindings(bindingsDraft.value)
    const message = err instanceof Error ? err.message : "Failed to save bindings"
    error.value = message
    toastStore.error(message)
  }
}

function isValidSignalSelection(diCount: number, doCount: number): boolean {
  return (diCount === 2 && doCount === 0) || (diCount === 2 && doCount === 2)
}

function validateSignalSelection(rows: SignalAllocationRow[]) {
  const diRows = rows.filter(row => row.signal_direction === "DI")
  const doRows = rows.filter(row => row.signal_direction === "DO")

  if (!isValidSignalSelection(diRows.length, doRows.length)) {
    throw new Error("Select either exactly 2 DI signals, or 2 DI + 2 DO signals")
  }

  const allocatedUnits = new Set(
    rows
      .map(row => String(row.unit_id ?? "").trim())
      .filter(unitId => unitId.length > 0),
  )

  if (allocatedUnits.size > 1) {
    throw new Error("Selected allocated signals must belong to one unit_id")
  }
}

function resolveBindingChannelId(role: BindingRoleKey): number | null {
  const channelId = Number(bindingsDraft.value.find(binding => binding.role === role)?.channel_id)
  return Number.isFinite(channelId) && channelId > 0 ? channelId : null
}

async function applySignalSelection() {
  applyingSignalSelection.value = true
  error.value = null

  try {
    const rows = [...selectedSignalRows.value]
    validateSignalSelection(rows)

    const diRows = rows.filter(row => row.signal_direction === "DI")
    const doRows = rows.filter(row => row.signal_direction === "DO")

    const roleToSignal: Array<{ role: BindingRoleKey; row: SignalAllocationRow }> = [
      { role: "do_open", row: diRows[0] },
      { role: "do_closed", row: diRows[1] },
    ]

    if (doRows.length === 2) {
      roleToSignal.push(
        { role: "di_open", row: doRows[0] },
        { role: "di_close", row: doRows[1] },
      )
    }

    const allocationEntries: Array<{ signal_id: number; channel_id: number | null }> = []
    const nextBindings = bindingsDraft.value.map(binding => ({ ...binding }))

    for (const item of roleToSignal) {
      const signalId = Number(item.row.signal_id)
      if (!Number.isFinite(signalId) || signalId <= 0) {
        throw new Error(`Invalid signal selected for ${ROLE_META[item.role].label}`)
      }

      const allocatedChannelId = Number(item.row.channel_id)
      const hasAllocatedChannel = Number.isFinite(allocatedChannelId) && allocatedChannelId > 0

      if (hasAllocatedChannel) {
        const target = nextBindings.find(binding => binding.role === item.role)
        if (target) {
          target.channel_id = allocatedChannelId
        }
        continue
      }

      const switchgearChannelId = resolveBindingChannelId(item.role)
      if (!switchgearChannelId) {
        throw new Error(`Switchgear channel for ${ROLE_META[item.role].label} is not configured`)
      }

      allocationEntries.push({
        signal_id: signalId,
        channel_id: switchgearChannelId,
      })
    }

    validateNoDuplicateChannels(nextBindings)

    if (allocationEntries.length > 0) {
      await signalSheetStore.bulkSetAllocations(allocationEntries)
    }

    await updateBindings(nextBindings)
    toastStore.success("Signal selection applied")
  } catch (err) {
    error.value = err instanceof Error ? err.message : "Failed to apply selected signals"
  } finally {
    applyingSignalSelection.value = false
  }
}

function openSignalPicker() {
  signalModalOpen.value = true
}

function clearSignalSelection() {
  selectedSignalRows.value = []
}

async function handleSignalPickerConfirm(rows: SignalAllocationRow[]) {
  signalModalOpen.value = false
  selectedSignalRows.value = rows
  error.value = null
  await applySignalSelection()
}

async function resetAll() {
  try {
    await store.resetBindings(props.switchgear.id)
    const next = normalizeBindings(props.switchgear.bindings)
    bindingsDraft.value = next
    syncSignalSelectionsFromBindings(next)
    selectedSignalRows.value = []
    error.value = null
    toastStore.success("Bindings reset")
  } catch (err) {
    error.value = err instanceof Error ? err.message : "Failed to reset bindings"
  }
}

function bindingModeHelpText() {
  return [
    "Direct",
    "Assign channel pairs directly for DO/DI groups.",
    "",
    "By signal",
    "Pick signals once in a single list and apply them to switchgear roles.",
    "Rules: either 2 DI signals, or 2 DI + 2 DO signals.",
  ].join("\n")
}

watch(
  () => [
    props.switchgear.id,
    (props.switchgear.bindings ?? [])
      .map(binding => `${binding.role}:${binding.channel_id ?? "n"}:${binding.delay_ms ?? 0}`)
      .join("|"),
  ] as const,
  () => {
    const next = normalizeBindings(props.switchgear.bindings)
    bindingsDraft.value = next
    syncSignalSelectionsFromBindings(next)
    error.value = null
    applyingSignalSelection.value = false
  },
  { immediate: true },
)

watch(
  () => props.switchgear.id,
  () => {
    bindingMode.value = "direct"
    selectedSignalRows.value = []
  },
)

watch(
  signalModeAvailable,
  (available) => {
    if (!available && bindingMode.value !== "direct") {
      bindingMode.value = "direct"
      selectedSignalRows.value = []
    }
  },
  { immediate: true },
)

  watch(
    () => workspaceStore.activeWorkspaceId,
    (workspaceId) => {
      if (!workspaceId) return
      void runStoreBootstrap(
        ["switchgear-bindings-editor-signal-sheet", workspaceId],
        [
          () => signalSheetStore.ensureSheetLoaded(),
          () => signalSheetStore.ensureAllocationsLoaded(),
        ],
        { mode: "settled" },
      )
    },
  { immediate: true },
)
</script>

<template>
  <div class="space-y-4 rounded-md border border-neutral-200 bg-neutral-50 p-4 dark:border-neutral-700 dark:bg-neutral-900">
    <div class="flex items-start justify-between gap-3">
      <div>
        <div class="text-xs uppercase tracking-wide text-neutral-500 dark:text-neutral-400">
          Editing bindings
        </div>
        <div class="text-sm font-semibold text-neutral-900 dark:text-white">
          Pair mapping (2 actions)
        </div>
        <div class="mt-2">
          <UiButton size="xs" variant="ghost" @click="resetAll">
            Reset
          </UiButton>
        </div>
      </div>

      <div class="flex items-center">
        <UiButton size="xs" variant="ghost" @click="emit('close')">
          ×
        </UiButton>
      </div>
    </div>

    <div class="flex items-center gap-2">
      <DirectSignalModeTabs
        :model-value="bindingMode"
        :show-signal="signalModeAvailable"
        aria-label="Binding mode"
        @update:model-value="setMode"
      />
      <InlineInfoTooltip :text="bindingModeHelpText()" aria-label="Binding mode help" />
    </div>

    <template v-if="bindingMode === 'signal'">
      <div class="rounded-md border border-neutral-200 bg-white p-3 dark:border-neutral-700 dark:bg-neutral-800">
        <div class="mb-2 text-xs font-semibold uppercase tracking-wide text-neutral-500 dark:text-neutral-400">
          Signals selection
        </div>

        <div class="mb-3 text-sm text-neutral-700 dark:text-neutral-200">
          {{ signalSelectionSummary }}
        </div>

        <div class="flex items-center gap-2">
          <UiButton size="sm" variant="secondary" @click="openSignalPicker">
            Pick signals
          </UiButton>
          <UiButton size="sm" variant="ghost" :disabled="selectedSignalRows.length === 0" @click="clearSignalSelection">
            Clear
          </UiButton>
        </div>
      </div>
    </template>

    <template v-else>
      <div class="grid grid-cols-1 gap-4 lg:grid-cols-2">
        <section class="rounded-md border border-neutral-200 bg-white p-3 dark:border-neutral-700 dark:bg-neutral-800">
          <div class="mb-3 text-xs font-semibold uppercase tracking-wide text-neutral-500 dark:text-neutral-400">
            indication
          </div>

          <div class="space-y-3">
            <div v-for="role in DO_ROLES" :key="role" class="space-y-1">
              <div class="text-xs text-neutral-500 dark:text-neutral-400">{{ ROLE_META[role].label }}</div>
              <SignalBackedChannelField
                :channel-id="channelValue(role)"
                :channel-type="ROLE_META[role].channelType"
                :exclude-ids="excludeIdsForRole(role)"
                :signal-id="signalSelectionByRole[role].signalId"
                :signal-key="signalSelectionByRole[role].signalKey"
                :name="`binding-${role}`"
                :signal-picker-title="`${ROLE_META[role].label} · Select signal`"
                :table-id="`switchgear-binding-${props.switchgear.id}-${role}`"
                :mode="bindingMode"
                :allow-signal-mode="signalModeAvailable"
                :show-mode-toggle="false"
                :show-signal-clear="false"
                :empty-signal-subtitle="''"
                :signal-display-mode="'source-row'"
                :signal-display-delimiter="' | '"
                :signal-label-scrollable="true"
                :show-signal-subtitle="false"
                @update:channelId="value => handleChannelChange(role, value)"
                @update:signal="value => handleSignalChange(role, value)"
              />
            </div>
          </div>
        </section>

        <section class="rounded-md border border-neutral-200 bg-white p-3 dark:border-neutral-700 dark:bg-neutral-800">
          <div class="mb-3 text-xs font-semibold uppercase tracking-wide text-neutral-500 dark:text-neutral-400">
            control from BCU
          </div>

          <div class="space-y-3">
            <div v-for="role in DI_ROLES" :key="role" class="space-y-1">
              <div class="text-xs text-neutral-500 dark:text-neutral-400">{{ ROLE_META[role].label }}</div>
              <SignalBackedChannelField
                :channel-id="channelValue(role)"
                :channel-type="ROLE_META[role].channelType"
                :exclude-ids="excludeIdsForRole(role)"
                :signal-id="signalSelectionByRole[role].signalId"
                :signal-key="signalSelectionByRole[role].signalKey"
                :name="`binding-${role}`"
                :signal-picker-title="`${ROLE_META[role].label} · Select signal`"
                :table-id="`switchgear-binding-${props.switchgear.id}-${role}`"
                :mode="bindingMode"
                :allow-signal-mode="signalModeAvailable"
                :show-mode-toggle="false"
                :show-signal-clear="false"
                :empty-signal-subtitle="''"
                :signal-display-mode="'source-row'"
                :signal-display-delimiter="' | '"
                :signal-label-scrollable="true"
                :show-signal-subtitle="false"
                @update:channelId="value => handleChannelChange(role, value)"
                @update:signal="value => handleSignalChange(role, value)"
              />

              <div class="flex items-center gap-2 text-xs text-neutral-500">
                <span class="uppercase tracking-wide text-[11px]">Feedback delay</span>
                <input
                  type="number"
                  min="0"
                  step="50"
                  class="w-24 rounded border border-neutral-300 bg-white px-2 py-1 text-neutral-900 dark:border-neutral-700 dark:bg-neutral-900 dark:text-neutral-200"
                  :name="`binding-delay-${role}`"
                  :value="delayFor(role)"
                  @change="event => handleDelayChange(role, Number((event.target as HTMLInputElement).value))"
                />
                <span>ms</span>
              </div>
            </div>
          </div>
        </section>
      </div>
    </template>

    <UiAlert v-if="error" type="error" :message="error" />
  </div>

  <SignalSelectionGridModal
    :open="signalModalOpen"
    title="Select switchgear signals"
    :allowed-directions="['DI', 'DO'] as SignalIODirection[]"
    :multiple="true"
    :min-selected="2"
    confirm-label="Use selected signals"
    :table-id="`switchgear-signals-${props.switchgear.id}`"
    @close="signalModalOpen = false"
    @confirm="handleSignalPickerConfirm"
  />
</template>
