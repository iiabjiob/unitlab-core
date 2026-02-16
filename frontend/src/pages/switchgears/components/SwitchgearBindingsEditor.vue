<script setup lang="ts">
import { computed, ref, watch } from "vue"
import type { Switchgear } from "@/types/switchgear"
import { CHANNEL_TYPES, type ChannelType } from "@/types/channel"
import SignalBackedChannelField from "@/components/signals/SignalBackedChannelField.vue"
import UiButton from "@/components/ui/UiButton.vue"
import { useSwitchgearStore } from "@/stores/switchgearStore"
import { useToastStore } from "@/stores/toastStore"
import { useSignalSheetStore } from "@/stores/signalSheetStore"
import { useWorkspaceStore } from "@/stores/workspaceStore"
import { useTabsController } from "@affino/tabs-vue"

const props = defineProps<{
  switchgear: Switchgear
}>()

const store = useSwitchgearStore()
const toastStore = useToastStore()
const signalSheetStore = useSignalSheetStore()
const workspaceStore = useWorkspaceStore()

const ROLE_ORDER = ["do_open", "do_closed", "di_open", "di_close"] as const
type BindingRoleKey = typeof ROLE_ORDER[number]
type BindingMode = "direct" | "signal"

const ROLE_META: Record<BindingRoleKey, {
  label: string
  channelType: ChannelType
  supportsDelay: boolean
}> = {
  do_open: {
    label: "Set OPEN position",
    channelType: CHANNEL_TYPES.DO,
    supportsDelay: false,
  },
  do_closed: {
    label: "Set CLOSED position",
    channelType: CHANNEL_TYPES.DO,
    supportsDelay: false,
  },
  di_open: {
    label: "IED OPEN command",
    channelType: CHANNEL_TYPES.DI,
    supportsDelay: true,
  },
  di_close: {
    label: "IED CLOSE command",
    channelType: CHANNEL_TYPES.DI,
    supportsDelay: true,
  },
}

type EditableBinding = Switchgear["bindings"][number]

function normalizeBindings(source: ReadonlyArray<EditableBinding> | null | undefined): EditableBinding[] {
  return ROLE_ORDER.map((role) => {
    const current = source?.find(binding => binding.role === role) ?? null
    return {
      id: current?.id ?? 0,
      role,
      channel_id: current?.channel_id ?? null,
      delay_ms: current?.delay_ms ?? 0,
    }
  })
}

const bindingsDraft = ref<EditableBinding[]>(normalizeBindings(props.switchgear.bindings))
const signalSelectionByRole = ref<Record<BindingRoleKey, { signalId: number | null; signalKey: string | null }>>({
  do_open: { signalId: null, signalKey: null },
  do_closed: { signalId: null, signalKey: null },
  di_open: { signalId: null, signalKey: null },
  di_close: { signalId: null, signalKey: null },
})
const bindingModeTabs = useTabsController<BindingMode>("direct")
const bindingMode = computed<BindingMode>(() => (
  bindingModeTabs.state.value.value === "signal" ? "signal" : "direct"
))
const signalModeAvailable = computed(() => {
  const workspaceId = workspaceStore.activeWorkspaceId
  const sheet = signalSheetStore.sheet
  if (!workspaceId || !sheet) return false
  return sheet.workspace_id === workspaceId && sheet.signals_count > 0
})

function bindingByRole(role: BindingRoleKey) {
  return bindingsDraft.value.find(binding => binding.role === role) ?? null
}

async function updateBindings(nextBindings: EditableBinding[]) {
  const normalized = normalizeBindings(nextBindings)
  bindingsDraft.value = normalized
  try {
    const updated = await store.updateField(props.switchgear.id, {
      bindings: normalized.map(binding => ({
        role: binding.role,
        channel_id: binding.channel_id,
        delay_ms: binding.delay_ms,
      })),
    })
    bindingsDraft.value = normalizeBindings(updated.bindings)
  } catch (error) {
    bindingsDraft.value = normalizeBindings(props.switchgear.bindings)
    throw error
  }
}

function patchBinding(role: BindingRoleKey, patch: Partial<{ channel_id: number | null; delay_ms: number }>) {
  return bindingsDraft.value.map(binding =>
    binding.role === role ? { ...binding, ...patch } : binding,
  )
}

function applyPatch(role: BindingRoleKey, patch: Partial<{ channel_id: number | null; delay_ms: number }>) {
  const next = patchBinding(role, patch)
  void updateBindings(next).catch((error) => {
    toastStore.error(error instanceof Error ? error.message : "Failed to update switchgear bindings")
  })
}

function handleDelayChange(role: BindingRoleKey, value: number) {
  const safeValue = Number.isFinite(value) ? Math.max(0, Math.round(value)) : 0
  applyPatch(role, { delay_ms: safeValue })
}

function delayFor(role: BindingRoleKey) {
  return bindingByRole(role)?.delay_ms ?? 0
}

function channelValue(role: BindingRoleKey) {
  return bindingByRole(role)?.channel_id ?? null
}

function roleLabel(role: BindingRoleKey) {
  return ROLE_META[role].label
}

function signalPickerTitle(role: BindingRoleKey) {
  return `${roleLabel(role)} · Select signal`
}

function selectedSignalId(role: BindingRoleKey): number | null {
  return signalSelectionByRole.value[role]?.signalId ?? null
}

function selectedSignalKey(role: BindingRoleKey): string | null {
  return signalSelectionByRole.value[role]?.signalKey ?? null
}

function handleChannelChange(role: BindingRoleKey, channelId: number | null) {
  applyPatch(role, { channel_id: channelId })
}

function handleSignalChange(role: BindingRoleKey, payload: { signalId: number | null; signalKey: string | null }) {
  signalSelectionByRole.value[role] = payload
}

function setBindingMode(mode: BindingMode) {
  if (mode === "signal" && !signalModeAvailable.value) {
    return
  }
  bindingModeTabs.select(mode)
}

async function resetAll() {
  await store.resetBindings(props.switchgear.id)
  bindingsDraft.value = normalizeBindings(props.switchgear.bindings)
  signalSelectionByRole.value = {
    do_open: { signalId: null, signalKey: null },
    do_closed: { signalId: null, signalKey: null },
    di_open: { signalId: null, signalKey: null },
    di_close: { signalId: null, signalKey: null },
  }
}

watch(
  () => [
    props.switchgear.id,
    (props.switchgear.bindings ?? [])
      .map(binding => `${binding.role}:${binding.channel_id ?? "n"}:${binding.delay_ms ?? 0}`)
      .join("|"),
  ] as const,
  () => {
    bindingsDraft.value = normalizeBindings(props.switchgear.bindings)
  },
  { immediate: true },
)

watch(
  () => props.switchgear.id,
  () => {
    signalSelectionByRole.value = {
      do_open: { signalId: null, signalKey: null },
      do_closed: { signalId: null, signalKey: null },
      di_open: { signalId: null, signalKey: null },
      di_close: { signalId: null, signalKey: null },
    }
    bindingModeTabs.select("direct")
  },
)

watch(
  signalModeAvailable,
  (available) => {
    if (!available && bindingMode.value !== "direct") {
      bindingModeTabs.select("direct")
    }
  },
  { immediate: true },
)

watch(
  () => workspaceStore.activeWorkspaceId,
  (workspaceId) => {
    if (!workspaceId) return
    void signalSheetStore.refreshSheet().catch(() => undefined)
  },
  { immediate: true },
)
</script>

<template>
  <div class="h-full flex flex-col">
    <div class="flex items-center justify-between mb-4">
      <div class="text-xs uppercase tracking-wider text-neutral-500">Bindings</div>
      <UiButton size="xs" variant="ghost" @click="resetAll">
        Reset
      </UiButton>
    </div>

    <div class="mb-3 flex items-center gap-2" role="tablist" aria-label="Binding mode">
      <button
        type="button"
        role="tab"
        :aria-selected="bindingMode === 'direct'"
        class="rounded-md border px-3 py-1.5 text-xs font-semibold leading-none transition-colors"
        :class="bindingMode === 'direct'
          ? 'border-neutral-950 bg-neutral-950 text-white dark:border-white dark:bg-white dark:text-neutral-900'
          : 'border-neutral-500 bg-neutral-100 text-neutral-900 dark:border-neutral-400 dark:bg-neutral-800 dark:text-neutral-100'"
        @click="setBindingMode('direct')"
      >
        Direct
      </button>
      <button
        v-if="signalModeAvailable"
        type="button"
        role="tab"
        :aria-selected="bindingMode === 'signal'"
        class="rounded-md border px-3 py-1.5 text-xs font-semibold leading-none transition-colors"
        :class="bindingMode === 'signal'
          ? 'border-neutral-950 bg-neutral-950 text-white dark:border-white dark:bg-white dark:text-neutral-900'
          : 'border-neutral-500 bg-neutral-100 text-neutral-900 dark:border-neutral-400 dark:bg-neutral-800 dark:text-neutral-100'"
        @click="setBindingMode('signal')"
      >
        By Signal
      </button>
    </div>

    <div class="space-y-3 overflow-y-auto pr-1">
      <div
        v-for="role in ROLE_ORDER"
        :key="role"
        class="border border-neutral-200 dark:border-neutral-700 rounded-md p-3 flex flex-col gap-2"
      >
        <div class="flex items-center justify-between">
          <div class="text-sm font-medium">{{ roleLabel(role) }}</div>
          <!-- <div class="text-[11px] tracking-wide text-neutral-500">
            <span class="uppercase">{{ role }}</span>
          </div> -->
        </div>

        <SignalBackedChannelField
          :channel-id="channelValue(role)"
          :channel-type="ROLE_META[role].channelType"
          :signal-id="selectedSignalId(role)"
          :signal-key="selectedSignalKey(role)"
          :name="`binding-${role}`"
          :signal-picker-title="signalPickerTitle(role)"
          :table-id="`switchgear-binding-${props.switchgear.id}-${role}`"
          :mode="bindingMode"
          :allow-signal-mode="signalModeAvailable"
          :show-mode-toggle="false"
          :show-signal-clear="false"
          :empty-signal-subtitle="''"
          @update:channelId="value => handleChannelChange(role, value)"
          @update:signal="value => handleSignalChange(role, value)"
        />

        <div v-if="ROLE_META[role].supportsDelay" class="flex items-center gap-2 text-xs text-neutral-500">
          <label
            class="uppercase tracking-wide text-[11px]"
            :for="`binding-delay-${role}`"
          >
            Feedback delay
          </label>
          <input
            type="number"
            min="0"
            step="50"
            class="w-24 px-2 py-1 rounded border border-neutral-300 dark:border-neutral-700 bg-white dark:bg-neutral-900 text-neutral-900 dark:text-neutral-200"
            :id="`binding-delay-${role}`"
            :name="`binding-delay-${role}`"
            :value="delayFor(role)"
            @change="event => handleDelayChange(role, Number((event.target as HTMLInputElement).value))"
          />
          <span>ms</span>
        </div>
      </div>
    </div>
  </div>
</template>
