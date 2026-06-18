<script setup lang="ts">
import { computed, ref, watch } from "vue"

import type { Switchgear } from "@/types/switchgear"
import { CHANNEL_TYPES, type ChannelType } from "@/types/channel"
import { useChannelStore } from "@/stores/channelStore"
import { useSignalSheetStore } from "@/stores/signalSheetStore"
import { useWorkspaceStore } from "@/stores/workspaceStore"
import { useSwitchgearStore } from "@/stores/switchgearStore"
import { useToastStore } from "@/stores/toastStore"
import { runStoreBootstrap } from "@/composables/useStoreBootstrap"
import { extractSourceRowFromSignalMetadata } from "@/pages/signals/utils/sourceColumns"
import UiButton from "@/components/ui/UiButton.vue"

const props = defineProps<{
  switchgear: Switchgear
}>()

const emit = defineEmits<{
  (e: "edit"): void
}>()

const channelStore = useChannelStore()
const signalSheetStore = useSignalSheetStore()
const workspaceStore = useWorkspaceStore()
const switchgearStore = useSwitchgearStore()
const toastStore = useToastStore()

type BindingRoleKey = "do_open" | "do_closed" | "di_open" | "di_close"

const ROLE_META: Record<BindingRoleKey, {
  label: string
  channelType: ChannelType
}> = {
  do_open: { label: "OPEN position", channelType: CHANNEL_TYPES.DO },
  do_closed: { label: "CLOSED position", channelType: CHANNEL_TYPES.DO },
  di_open: { label: "OPEN command from BCU", channelType: CHANNEL_TYPES.DI },
  di_close: { label: "CLOSE command from BCU", channelType: CHANNEL_TYPES.DI },
}

const GROUPS: Array<{ id: "do" | "di"; title: string; roles: BindingRoleKey[] }> = [
  { id: "do", title: "indication", roles: ["do_open", "do_closed"] },
  { id: "di", title: "control from BCU", roles: ["di_open", "di_close"] },
]

const delayDraft = ref<Record<BindingRoleKey, number>>({
  do_open: 0,
  do_closed: 0,
  di_open: 0,
  di_close: 0,
})

const savingDelay = ref<Record<BindingRoleKey, boolean>>({
  do_open: false,
  do_closed: false,
  di_open: false,
  di_close: false,
})

function channelIdForRole(role: BindingRoleKey): number | null {
  const binding = props.switchgear.bindings.find(item => item.role === role)
  const channelId = Number(binding?.channel_id)
  return Number.isFinite(channelId) && channelId > 0 ? channelId : null
}

function channelByRole(role: BindingRoleKey) {
  const channelId = channelIdForRole(role)
  if (!channelId) return null
  const channel = channelStore.channels.find(item => item.id === channelId) ?? null
  if (!channel || channel.type !== ROLE_META[role].channelType) {
    return null
  }
  return channel
}

function rowForChannel(role: BindingRoleKey) {
  const channel = channelByRole(role)
  if (!channel) return null
  return signalSheetStore.allocationRows.find((row) => {
    const rowChannelId = Number(row.channel_id)
    if (!Number.isFinite(rowChannelId) || rowChannelId <= 0) return false
    if (rowChannelId !== channel.id) return false
    const rowType = String(row.channel_type ?? "").trim().toLowerCase()
    return rowType.startsWith(ROLE_META[role].channelType)
  }) ?? null
}

function stringifyValue(value: unknown): string {
  if (value === null || value === undefined) return ""
  if (typeof value === "string") return value.trim()
  if (typeof value === "number" || typeof value === "boolean") return String(value)
  if (Array.isArray(value)) {
    return value
      .map(item => stringifyValue(item))
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
  return ""
}

function signalLine(role: BindingRoleKey): string {
  const row = rowForChannel(role)
  if (!row) return ""

  const sourceRow = extractSourceRowFromSignalMetadata(row.signal_metadata)
  const segments = Object.values(sourceRow)
    .map(value => stringifyValue(value))
    .filter(value => value.length > 0)

  if (segments.length > 0) {
    return segments.join(" | ")
  }

  return row.signal_name || row.signal_key || ""
}

function bindingLine(role: BindingRoleKey): string {
  const channel = channelByRole(role)
  if (!channel) return "Not assigned"
  return channelStore.resolveChannelFullLabel(channel)
}

function delayForRole(role: BindingRoleKey): number {
  const binding = props.switchgear.bindings.find(item => item.role === role)
  const delay = Number(binding?.delay_ms)
  return Number.isFinite(delay) ? Math.max(0, Math.round(delay)) : 0
}

function showFeedbackDelay(role: BindingRoleKey): boolean {
  return role === "di_open" || role === "di_close"
}

function syncDelayDraftFromSwitchgear() {
  const next: Record<BindingRoleKey, number> = {
    do_open: 0,
    do_closed: 0,
    di_open: 0,
    di_close: 0,
  }

  props.switchgear.bindings.forEach((binding) => {
    const role = binding.role as BindingRoleKey
    if (!(role in next)) return
    const delay = Number(binding.delay_ms)
    next[role] = Number.isFinite(delay) ? Math.max(0, Math.round(delay)) : 0
  })

  delayDraft.value = next
}

async function saveDelay(role: BindingRoleKey) {
  if (!showFeedbackDelay(role)) return

  const nextDelay = Number.isFinite(delayDraft.value[role])
    ? Math.max(0, Math.round(delayDraft.value[role]))
    : 0

  delayDraft.value[role] = nextDelay

  if (nextDelay === delayForRole(role)) {
    return
  }

  savingDelay.value = {
    ...savingDelay.value,
    [role]: true,
  }

  try {
    const bindingsPayload = props.switchgear.bindings.map(binding => ({
      role: binding.role,
      channel_id: binding.channel_id,
      delay_ms: binding.role === role ? nextDelay : (Number.isFinite(binding.delay_ms as number) ? Math.max(0, Math.round(Number(binding.delay_ms))) : 0),
    }))

    await switchgearStore.updateField(props.switchgear.id, {
      bindings: bindingsPayload,
    })
  } catch (err) {
    const message = err instanceof Error ? err.message : "Failed to update feedback delay"
    toastStore.error(message)
    syncDelayDraftFromSwitchgear()
  } finally {
    savingDelay.value = {
      ...savingDelay.value,
      [role]: false,
    }
  }
}

const hasAnyBinding = computed(() => (
  GROUPS.some(group => group.roles.some(role => channelIdForRole(role) !== null))
))

watch(
  () => workspaceStore.activeWorkspaceId,
  (workspaceId) => {
    if (!workspaceId) return
    void runStoreBootstrap(
      ["switchgear-summary-signal-rows", workspaceId],
      [
        () => channelStore.ensureLoaded(),
        () => signalSheetStore.refreshAllocations(),
      ],
      { mode: "settled" },
    )
  },
  { immediate: true },
)

watch(
  () => props.switchgear.bindings,
  () => {
    syncDelayDraftFromSwitchgear()
  },
  { immediate: true, deep: true },
)
</script>

<template>
  <div class="switchgear-bindings-summary">
    <div class="switchgear-bindings-summary__header">
      <div>
        <div class="switchgear-bindings-summary__eyebrow">Bindings summary</div>
        <div class="switchgear-bindings-summary__subtitle">Current channel usage</div>
      </div>
      <UiButton size="xs" variant="secondary" @click="emit('edit')">
        Edit bindings
      </UiButton>
    </div>

    <div v-if="!hasAnyBinding" class="switchgear-bindings-summary__empty">
      No bindings configured yet.
    </div>

    <div class="switchgear-bindings-summary__groups">
      <div
        v-for="group in GROUPS"
        :key="group.id"
        class="switchgear-bindings-summary__group"
      >
        <div class="switchgear-bindings-summary__group-title">
          {{ group.title }}
        </div>

        <div class="switchgear-bindings-summary__role-list">
          <div
            v-for="role in group.roles"
            :key="role"
            class="switchgear-bindings-summary__role-card"
          >
            <div class="switchgear-bindings-summary__role-label">
              {{ ROLE_META[role].label }}
            </div>
            <div class="switchgear-bindings-summary__binding">
              {{ bindingLine(role) }}
            </div>
            <div
              v-if="signalLine(role)"
              class="switchgear-bindings-summary__signal"
              :title="signalLine(role)"
            >
              {{ signalLine(role) }}
            </div>
            <div
              v-if="showFeedbackDelay(role)"
              class="switchgear-bindings-summary__delay"
            >
              <span>Feedback delay</span>
              <input
                type="number"
                autocomplete="off"
                min="0"
                step="50"
                class="switchgear-bindings-summary__delay-input"
                :name="`summary-feedback-delay-${role}`"
                :value="delayDraft[role]"
                :disabled="savingDelay[role]"
                @input="event => { delayDraft[role] = Number((event.target as HTMLInputElement).value) }"
                @change="() => void saveDelay(role)"
              />
              <span>ms</span>
            </div>
          </div>
        </div>
      </div>
    </div>
  </div>
</template>

<style scoped>
.switchgear-bindings-summary {
  display: flex;
  height: 100%;
  min-height: 0;
  flex-direction: column;
  padding: 1rem;
}

.switchgear-bindings-summary__header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  margin-bottom: 1rem;
}

.switchgear-bindings-summary__eyebrow {
  color: var(--color-neutral-500);
  font-size: var(--text-xs);
  letter-spacing: 0;
  text-transform: uppercase;
}

.switchgear-bindings-summary__subtitle {
  color: var(--color-neutral-600);
  font-size: var(--text-sm);
}

.switchgear-bindings-summary__empty {
  padding: 0.5rem 0.75rem;
  border: 1px dashed var(--color-neutral-300);
  border-radius: var(--radius-md);
  color: var(--color-neutral-500);
  font-size: var(--text-xs);
  margin-bottom: 1rem;
}

.switchgear-bindings-summary__groups {
  display: flex;
  flex-direction: column;
  gap: 0.75rem;
  padding-right: 0.25rem;
}

.switchgear-bindings-summary__group {
  padding: 0.75rem;
  border: 1px solid var(--color-neutral-200);
  border-radius: var(--radius-md);
}

.switchgear-bindings-summary__group-title {
  margin-bottom: 0.5rem;
  color: var(--color-neutral-500);
  font-size: var(--text-xs);
  font-weight: 600;
  letter-spacing: 0;
  text-transform: uppercase;
}

.switchgear-bindings-summary__role-list {
  display: flex;
  flex-direction: column;
  gap: 0.5rem;
}

.switchgear-bindings-summary__role-card {
  padding: 0.5rem 0.75rem;
  border: 1px solid var(--color-neutral-200);
  border-radius: var(--radius-sm);
  background: var(--color-neutral-50);
}

.switchgear-bindings-summary__role-label {
  color: var(--color-neutral-500);
  font-size: var(--text-xs);
  letter-spacing: 0;
  text-transform: uppercase;
}

.switchgear-bindings-summary__binding {
  color: var(--color-neutral-900);
  font-size: var(--text-sm);
  font-weight: 500;
}

.switchgear-bindings-summary__signal {
  margin-top: 0.25rem;
  overflow-x: auto;
  color: var(--color-neutral-600);
  font-size: var(--text-xs);
  scrollbar-width: none;
  white-space: nowrap;
}

.switchgear-bindings-summary__signal::-webkit-scrollbar {
  display: none;
}

.switchgear-bindings-summary__delay {
  display: flex;
  align-items: center;
  gap: 0.5rem;
  margin-top: 0.25rem;
  color: var(--color-neutral-600);
  font-size: var(--text-xs);
}

.switchgear-bindings-summary__delay-input {
  width: 6rem;
  padding: 0.25rem 0.5rem;
  border: 1px solid var(--color-neutral-300);
  border-radius: var(--radius-sm);
  background: var(--color-white);
  color: var(--color-neutral-900);
}

.switchgear-bindings-summary__delay-input:disabled {
  opacity: 0.65;
}

@media (min-width: 1024px) {
  .switchgear-bindings-summary__groups {
    flex: 1 1 auto;
    min-height: 0;
    overflow-y: auto;
  }
}

:global(.dark .switchgear-bindings-summary__subtitle),
:global(.dark .switchgear-bindings-summary__signal),
:global(.dark .switchgear-bindings-summary__delay) {
  color: var(--color-neutral-300);
}

:global(.dark .switchgear-bindings-summary__empty),
:global(.dark .switchgear-bindings-summary__group-title),
:global(.dark .switchgear-bindings-summary__role-label) {
  color: var(--color-neutral-400);
}

:global(.dark .switchgear-bindings-summary__empty),
:global(.dark .switchgear-bindings-summary__group),
:global(.dark .switchgear-bindings-summary__role-card) {
  border-color: var(--color-neutral-700);
}

:global(.dark .switchgear-bindings-summary__role-card) {
  background: var(--color-neutral-900);
}

:global(.dark .switchgear-bindings-summary__binding) {
  color: var(--color-neutral-100);
}

:global(.dark .switchgear-bindings-summary__delay-input) {
  border-color: var(--color-neutral-700);
  background: var(--color-neutral-900);
  color: var(--color-neutral-200);
}
</style>
