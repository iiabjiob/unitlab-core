<script setup lang="ts">
import { computed } from "vue"
import type { Switchgear } from "@/types/switchgear"
import { CHANNEL_TYPES, type ChannelType } from "@/types/channel"
import ChannelSelect from "@/components/ui/ChannelSelect.vue"
import UiButton from "@/components/ui/UiButton.vue"
import { useSwitchgearStore } from "@/stores/switchgearStore"

const props = defineProps<{
  switchgear: Switchgear
}>()

const store = useSwitchgearStore()

const ROLE_ORDER = ["do_open", "do_closed", "di_open", "di_close"] as const
type BindingRoleKey = typeof ROLE_ORDER[number]

const ROLE_META: Record<BindingRoleKey, {
  label: string
  channelType: ChannelType
  hint: string
  supportsDelay: boolean
}> = {
  do_open: {
    label: "Open command",
    channelType: CHANNEL_TYPES.DO,
    hint: "Pulse DO channel to open",
    supportsDelay: true,
  },
  do_closed: {
    label: "Close command",
    channelType: CHANNEL_TYPES.DO,
    hint: "Pulse DO channel to close",
    supportsDelay: true,
  },
  di_open: {
    label: "Open feedback",
    channelType: CHANNEL_TYPES.DI,
    hint: "DI channel indicating open state",
    supportsDelay: false,
  },
  di_close: {
    label: "Close feedback",
    channelType: CHANNEL_TYPES.DI,
    hint: "DI channel indicating closed state",
    supportsDelay: false,
  },
}

const bindings = computed(() => props.switchgear.bindings ?? [])

function bindingByRole(role: BindingRoleKey) {
  return bindings.value.find(binding => binding.role === role) ?? null
}

async function updateBindings(nextBindings: typeof bindings.value) {
  await store.updateField(props.switchgear.id, {
    bindings: nextBindings.map(binding => ({
      role: binding.role,
      channel_id: binding.channel_id,
      delay_ms: binding.delay_ms,
    })),
  })
}

function applyPatch(role: BindingRoleKey, patch: Partial<{ channel_id: number | null; delay_ms: number }>) {
  const next = bindings.value.map(binding =>
    binding.role === role ? { ...binding, ...patch } : binding
  )
  void updateBindings(next)
}

function handleChannelChange(role: BindingRoleKey, value: number | string | null) {
  if (value === null || value === "") {
    applyPatch(role, { channel_id: null })
    return
  }
  const numeric = typeof value === "number" ? value : Number(value)
  applyPatch(role, { channel_id: Number.isNaN(numeric) ? null : numeric })
}

function handleDelayChange(role: BindingRoleKey, value: number) {
  const safeValue = Number.isFinite(value) ? Math.max(0, Math.round(value)) : 0
  applyPatch(role, { delay_ms: safeValue })
}

function clearChannel(role: BindingRoleKey) {
  applyPatch(role, { channel_id: null })
}

function delayFor(role: BindingRoleKey) {
  return bindingByRole(role)?.delay_ms ?? 0
}

function channelValue(role: BindingRoleKey) {
  return bindingByRole(role)?.channel_id ?? null
}

function roleHint(role: BindingRoleKey) {
  return ROLE_META[role].hint
}

function roleLabel(role: BindingRoleKey) {
  return ROLE_META[role].label
}

async function resetAll() {
  await store.resetBindings(props.switchgear.id)
}
</script>

<template>
  <div class="h-full flex flex-col">
    <div class="flex items-center justify-between mb-4">
      <div class="text-xs uppercase tracking-wider text-neutral-500">Bindings</div>
      <UiButton size="xs" variant="ghost" @click="resetAll">
        Reset
      </UiButton>
    </div>

    <div class="space-y-3 overflow-y-auto pr-1">
      <div
        v-for="role in ROLE_ORDER"
        :key="role"
        class="border border-neutral-200 dark:border-neutral-700 rounded-md p-3 flex flex-col gap-2"
      >
        <div class="flex items-center justify-between">
          <div class="text-sm font-medium">{{ roleLabel(role) }}</div>
          <div class="text-[11px] uppercase tracking-wide text-neutral-500">{{ role }}</div>
        </div>

        <ChannelSelect
          :model-value="channelValue(role)"
          :channel-type="ROLE_META[role].channelType"
          :name="`binding-${role}`"
          @update:modelValue="value => handleChannelChange(role, value)"
        />

        <div v-if="ROLE_META[role].supportsDelay" class="flex items-center gap-2 text-xs text-neutral-500">
          <label
            class="uppercase tracking-wide text-[11px]"
            :for="`binding-delay-${role}`"
          >
            Pulse Duration
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

        <div class="flex items-center justify-between text-[11px] text-neutral-500">
          <span>{{ roleHint(role) }}</span>
          <UiButton size="xs" variant="ghost" @click="clearChannel(role)">
            Clear
          </UiButton>
        </div>
      </div>
    </div>
  </div>
</template>
