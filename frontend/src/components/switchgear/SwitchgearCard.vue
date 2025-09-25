<!-- src/components/switchgear/SwitchgearCard.vue -->
<template>
  <div class="flex flex-col gap-3">
    <!-- Header -->
    <div class="flex items-center justify-between">
      <div class="text-sm font-medium text-neutral-700 dark:text-neutral-200">
        {{ title }}
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

    <!-- Tech footer -->
    <SwitchgearTechFooter
      :do-open="doOpenResolved"
      :do-closed="doClosedResolved"
      :di-open="diOpenResolved"
      :di-close="diCloseResolved"
      :unit-online="unitOnline"
    />
  </div>
</template>

<script setup lang="ts">
import { computed, type Ref } from "vue"
import { useSwitchgear, type ChannelRef } from "@/composables/useSwitchgear"
import { useChannelStore } from "@/stores/channelStore"
import { useValidationStore } from "@/stores/validationStore"

import SwitchgearActions from "./SwitchgearActions.vue"
import SwitchgearCube from "./SwitchgearCube.vue"
import SwitchgearTechFooter from "./SwitchgearTechFooter.vue"
import SwitchgearMenu from "./SwitchgearMenu.vue"

import { useDeviceStore } from "@/stores/deviceStore"

const deviceStore = useDeviceStore()

const unitOnline = computed(() => {
  const anyCh = [props.do_open, props.do_closed].find(Boolean)
  if (!anyCh) return true

  const ch = channelStore.channels.find(c => c.id === anyCh)
  if (!ch) return true

  const dev = deviceStore.devices.find(d => d.id === ch.device_id)
  return dev?.status === "online"
})

const displayState = computed(() => {
  return unitOnline.value ? effectiveState.value : "UNKNOWN"
})

const props = withDefaults(
  defineProps<{
    id: number
    title: string
    do_open: number | null
    do_closed: number | null
    di_open: number | null
    di_close: number | null
    selected?: boolean
  }>(),
  {
    title: "2-Pos Switchgear",
    do_open: null,
    do_closed: null,
    di_open: null,
    di_close: null,
    selected: false,
  }
)

const emit = defineEmits<{
  (e: "delete"): void
}>()

// Resolve channel details from IDs
const channelStore = useChannelStore()

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

const doOpenResolved: Ref<ChannelRef | null> = computed(() => resolveChannel(props.do_open))
const doClosedResolved: Ref<ChannelRef | null> = computed(() => resolveChannel(props.do_closed))
const diOpenResolved: Ref<ChannelRef | null> = computed(() => resolveChannel(props.di_open))
const diCloseResolved: Ref<ChannelRef | null> = computed(() => resolveChannel(props.di_close))

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

const validation = useValidationStore()

const hasBlockingErrors = computed(() =>
  validation.errors.some(
    e => e.schemaName === "switchgear" && e.itemId === props.id && e.level !== "warning"
  )
)

const isDisabled = computed(() => isCmdDisabled || hasBlockingErrors.value)

</script>
