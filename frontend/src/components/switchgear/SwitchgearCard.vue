<!-- src/components/switchgear/SwitchgearCard.vue -->
<template>
  <div class="flex flex-col gap-3">
    <!-- Header -->
    <div class="flex items-center justify-between">
      <div class="text-sm font-medium text-neutral-700 dark:text-neutral-200">
        {{ props.switchgear.name }}
      </div>
      <div class="flex items-center gap-2">
        <SwitchgearMenu @delete="$emit('delete')" />
      </div>
    </div>

    <!-- Visual cube -->
    <SwitchgearCube
      :effective-state="unitOnline ? effectiveState : 'UNKNOWN'"
      :pending-target="unitOnline ? pendingTarget : null"
    />
    <div class="mx-auto">

      <span class="text-xs px-2 py-0.5 rounded-full" :class="statePillClass">
        {{ displayState }}
      </span>
    </div>

    <!-- Actions -->
    <SwitchgearActions
      :is-cmd-disabled="(s) => !unitOnline || isCmdDisabled(s)"
      :set-do-pair="setDoPair"
    />

    <!-- Mapping editor -->
    <div
      class="rounded-lg border border-neutral-200/70 dark:border-neutral-800/80 px-3 py-3 space-y-4"
    >
      <div>
        <p class="text-[10px] uppercase tracking-[0.2em] text-neutral-500 dark:text-neutral-400">
          Control · DO
        </p>
        <div class="mt-2 space-y-2">
          <div class="flex items-center gap-2 text-xs">
            <span class="w-16 font-medium text-neutral-600 dark:text-neutral-200">Open</span>
            <ChannelSelect
              class="flex-1"
              :model-value="doOpenChannelId"
              :channel-type="CHANNEL_TYPES.DO"
              name="do-open"
              @update:modelValue="value => handleChannelChange('do_open', value)"
            />
            <span class="w-24 text-[10px] uppercase tracking-wide text-neutral-400">—</span>
          </div>
          <div class="flex items-center gap-2 text-xs">
            <span class="w-16 font-medium text-neutral-600 dark:text-neutral-200">Closed</span>
            <ChannelSelect
              class="flex-1"
              :model-value="doClosedChannelId"
              :channel-type="CHANNEL_TYPES.DO"
              name="do-closed"
              @update:modelValue="value => handleChannelChange('do_closed', value)"
            />
            <span class="w-24 text-[10px] uppercase tracking-wide text-neutral-400">—</span>
          </div>
        </div>
      </div>

      <div>
        <p class="text-[10px] uppercase tracking-[0.2em] text-neutral-500 dark:text-neutral-400">
          Indication · DI
        </p>
        <div class="mt-2 space-y-2">
          <div class="flex items-center gap-2 text-xs">
            <span class="w-16 font-medium text-neutral-600 dark:text-neutral-200">Open</span>
            <ChannelSelect
              class="flex-1"
              :model-value="diOpenChannelId"
              :channel-type="CHANNEL_TYPES.DI"
              name="di-open"
              @update:modelValue="value => handleChannelChange('di_open', value)"
            />
            <label class="w-28 text-[10px] uppercase tracking-wide text-neutral-500 dark:text-neutral-400">
              Delay
              <input
                type="number"
                min="0"
                step="10"
                class="mt-1 w-full rounded border border-neutral-300/80 bg-white px-2 py-1 text-xs text-neutral-800 focus:outline-none focus:ring-1 focus:ring-sky-400 dark:border-neutral-700 dark:bg-neutral-900 dark:text-neutral-100"
                :value="diOpenDelayMs ?? 0"
                @change="event => handleDelayInput('di_open', event)"
              />
            </label>
          </div>
          <div class="flex items-center gap-2 text-xs">
            <span class="w-16 font-medium text-neutral-600 dark:text-neutral-200">Closed</span>
            <ChannelSelect
              class="flex-1"
              :model-value="diCloseChannelId"
              :channel-type="CHANNEL_TYPES.DI"
              name="di-closed"
              @update:modelValue="value => handleChannelChange('di_close', value)"
            />
            <label class="w-28 text-[10px] uppercase tracking-wide text-neutral-500 dark:text-neutral-400">
              Delay
              <input
                type="number"
                min="0"
                step="10"
                class="mt-1 w-full rounded border border-neutral-300/80 bg-white px-2 py-1 text-xs text-neutral-800 focus:outline-none focus:ring-1 focus:ring-sky-400 dark:border-neutral-700 dark:bg-neutral-900 dark:text-neutral-100"
                :value="diCloseDelayMs ?? 0"
                @change="event => handleDelayInput('di_close', event)"
              />
            </label>
          </div>
        </div>
      </div>
    </div>

  </div>
</template>

<script setup lang="ts">
import { computed, type Ref } from "vue"
import { useSwitchgear, type ChannelRef } from "@/composables/useSwitchgear"
import { useChannelStore } from "@/stores/channelStore"
import { useSwitchgearStore } from "@/stores/switchgearStore"
import type { Switchgear } from "@/types/switchgear"
import {
  SWITCHGEAR_BINDING_ROLES,
  type SwitchgearBindingInput,
  type SwitchgearBindingRole,
} from "@/types/switchgear"
import ChannelSelect from "@/components/ui/ChannelSelect.vue"
import { CHANNEL_TYPES } from "@/types/channel"

import SwitchgearActions from "./SwitchgearActions.vue"
import SwitchgearCube from "./SwitchgearCube.vue"
import SwitchgearMenu from "./SwitchgearMenu.vue"

import { useDeviceStore } from "@/stores/deviceStore"

const deviceStore = useDeviceStore()
const channelStore = useChannelStore()
const switchgearStore = useSwitchgearStore()

const unitOnline = computed(() => {
  const anyCh = [doOpenChannelId.value, doClosedChannelId.value].find(Boolean)
  if (!anyCh) return true

  const ch = channelStore.channels.find(c => c.id === anyCh)
  if (!ch) return true

  const dev = deviceStore.devices.find(d => d.id === ch.device_id)
  return dev?.status === "online"
})

const displayState = computed(() => {
  return unitOnline.value ? effectiveState.value : "UNKNOWN"
})

const props = defineProps<{
  switchgear: Switchgear
}>()

const emit = defineEmits<{
  (e: "delete"): void
}>()

function resolveChannel(chId: number | null): ChannelRef | null {
  if (!chId) return null
  const ch = channelStore.channels.find(c => c.id === chId)
  if (!ch) return null
  return {
    unitId: channelStore.resolveUnitId(ch.device_id),
    channel: ch.index,
    type: ch.type, // ок, попадёт в optional
  }
}

function channelIdForRole(role: SwitchgearBindingRole): number | null {
  return props.switchgear.bindings.find(binding => binding.role === role)?.channel_id ?? null
}

function bindingDelayMs(role: SwitchgearBindingRole): number | null {
  const binding = props.switchgear.bindings.find(binding => binding.role === role)
  return binding ? binding.delay_ms : null
}

function buildBindingsPayload(
  role: SwitchgearBindingRole,
  patch: Partial<Pick<SwitchgearBindingInput, "channel_id" | "delay_ms">>
): SwitchgearBindingInput[] {
  const bindingMap = new Map<string, SwitchgearBindingInput>()

  for (const binding of props.switchgear.bindings) {
    bindingMap.set(binding.role, {
      role: binding.role as SwitchgearBindingRole,
      channel_id: binding.channel_id ?? null,
      delay_ms: binding.delay_ms ?? 0,
    })
  }

  for (const roleKey of SWITCHGEAR_BINDING_ROLES) {
    if (!bindingMap.has(roleKey)) {
      bindingMap.set(roleKey, {
        role: roleKey,
        channel_id: null,
        delay_ms: 0,
      })
    }
  }

  const current = bindingMap.get(role) ?? { role, channel_id: null, delay_ms: 0 }
  bindingMap.set(role, { ...current, ...patch })

  return Array.from(bindingMap.values())
}

async function applyBindingUpdate(
  role: SwitchgearBindingRole,
  patch: Partial<Pick<SwitchgearBindingInput, "channel_id" | "delay_ms">>
) {
  const payload = buildBindingsPayload(role, patch)
  await switchgearStore.updateField(props.switchgear.id, { bindings: payload })
}

async function handleChannelChange(role: SwitchgearBindingRole, value: number | null) {
  if (channelIdForRole(role) === value) {
    return
  }
  await applyBindingUpdate(role, { channel_id: value })
}

function normalizeDelay(value: unknown): number {
  const num = typeof value === "number" ? value : Number(value)
  if (!Number.isFinite(num) || num <= 0) {
    return 0
  }
  return Math.round(num)
}

async function handleDelayInput(role: SwitchgearBindingRole, event: Event) {
  const target = event.target as HTMLInputElement
  const normalized = normalizeDelay(target.value)
  target.value = String(normalized)
  if ((bindingDelayMs(role) ?? 0) === normalized) {
    return
  }
  await applyBindingUpdate(role, { delay_ms: normalized })
}

const doOpenChannelId = computed(() => channelIdForRole("do_open"))
const doClosedChannelId = computed(() => channelIdForRole("do_closed"))
const diOpenChannelId = computed(() => channelIdForRole("di_open"))
const diCloseChannelId = computed(() => channelIdForRole("di_close"))

const diOpenDelayMs = computed(() => bindingDelayMs("di_open"))
const diCloseDelayMs = computed(() => bindingDelayMs("di_close"))

const doOpenResolved: Ref<ChannelRef | null> = computed(() => resolveChannel(doOpenChannelId.value))
const doClosedResolved: Ref<ChannelRef | null> = computed(() => resolveChannel(doClosedChannelId.value))
const diOpenResolved: Ref<ChannelRef | null> = computed(() => resolveChannel(diOpenChannelId.value))
const diCloseResolved: Ref<ChannelRef | null> = computed(() => resolveChannel(diCloseChannelId.value))

const {
  effectiveState,
  pendingTarget,
  busy,
  isCmdDisabled,
  setDoPair,
} = useSwitchgear({
  doOpen: doOpenResolved,
  doClosed: doClosedResolved,
  diOpen: diOpenResolved,
  diClose: diCloseResolved,
  diOpenDelayMs,
  diCloseDelayMs,
})

// Style helpers for state pill
const statePillClass = computed(() => {
  switch (displayState.value) {
    case "CLOSED":
      return "bg-neutral-900 text-white dark:bg-neutral-100 dark:text-neutral-900"
    case "OPEN":
      return "bg-neutral-100 text-neutral-700 dark:bg-neutral-800 dark:text-neutral-200"
    case "UNKNOWN":
      return "bg-amber-100 text-amber-800 dark:bg-amber-900/40 dark:text-amber-300"
    case "INTERMEDIATE":
      return "bg-sky-100 text-sky-800 dark:bg-sky-900/40 dark:text-sky-300"
  }
})

</script>
