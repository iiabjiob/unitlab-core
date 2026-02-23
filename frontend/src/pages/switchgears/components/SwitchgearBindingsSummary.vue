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
  <div class="h-full flex flex-col pe-4">
    <div class="mb-4 flex items-center justify-between">
      <div>
        <div class="text-xs uppercase tracking-wider text-neutral-500">Bindings summary</div>
        <div class="text-sm text-neutral-600 dark:text-neutral-300">Current channel usage</div>
      </div>
      <UiButton size="xs" variant="secondary" @click="emit('edit')">
        Edit bindings
      </UiButton>
    </div>

    <div v-if="!hasAnyBinding" class="rounded-md border border-dashed border-neutral-300 px-3 py-2 text-sm text-neutral-500 dark:border-neutral-700 dark:text-neutral-400">
      No bindings configured yet.
    </div>

    <div class="space-y-3 overflow-y-auto pr-1">
      <div
        v-for="group in GROUPS"
        :key="group.id"
        class="rounded-md border border-neutral-200 p-3 dark:border-neutral-700"
      >
        <div class="mb-2 text-xs font-semibold uppercase tracking-wide text-neutral-500 dark:text-neutral-400">
          {{ group.title }}
        </div>

        <div class="space-y-2">
          <div
            v-for="role in group.roles"
            :key="role"
            class="rounded border border-neutral-200 bg-neutral-50 px-3 py-2 dark:border-neutral-700 dark:bg-neutral-900"
          >
            <div class="text-xs uppercase tracking-wide text-neutral-500 dark:text-neutral-400">
              {{ ROLE_META[role].label }}
            </div>
            <div class="text-sm font-medium text-neutral-900 dark:text-neutral-100">
              {{ bindingLine(role) }}
            </div>
            <div
              v-if="signalLine(role)"
              class="mt-1 overflow-x-auto whitespace-nowrap text-xs text-neutral-600 [-ms-overflow-style:none] [scrollbar-width:none] [&::-webkit-scrollbar]:hidden dark:text-neutral-300"
              :title="signalLine(role)"
            >
              {{ signalLine(role) }}
            </div>
            <div
              v-if="showFeedbackDelay(role)"
              class="mt-1 flex items-center gap-2 text-xs text-neutral-600 dark:text-neutral-300"
            >
              <span>Feedback delay</span>
              <input
                type="number"
                min="0"
                step="50"
                class="w-24 rounded border border-neutral-300 bg-white px-2 py-1 text-neutral-900 dark:border-neutral-700 dark:bg-neutral-900 dark:text-neutral-200"
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
