<!-- src/components/switchgear/SwitchgearCard.vue -->
<template>

  <div class="flex flex-col gap-3">
    <!-- Header -->
    <div class="flex items-center justify-between">
      <div class="text-sm font-medium text-neutral-700 dark:text-neutral-200">
        {{ title }}
      </div>
      <div class="flex items-center gap-2">
        <span class="text-xs px-2 py-0.5 rounded-full" :class="statePillClass">{{ effectiveState }}</span>
        <SwitchgearMenu @delete="$emit('delete')" />
      </div>
    </div>

    <!-- Visual cube -->
    <SwitchgearCube :effective-state="effectiveState" :pending-target="pendingTarget" />

    <!-- Actions -->
    <SwitchgearActions :is-cmd-disabled="isCmdDisabled" :set-do-pair="setDoPair" />

    <!-- Tech footer -->
    <SwitchgearTechFooter
      :do-unit-id="doUnitId"
      :do-open-ch="doOpenCh"
      :do-close-ch="doCloseCh"
      :di-unit-id="diUnitId"
      :di-open-pulse-ch="diOpenPulseCh"
      :di-close-pulse-ch="diClosePulseCh"
    />
  </div>
</template>

<script setup lang="ts">
import { computed } from "vue"
import { useSwitchgear, type SwitchgearState } from "@/composables/useSwitchgear"
import SwitchgearActions from "./SwitchgearActions.vue"
import SwitchgearCube from "./SwitchgearCube.vue"
import SwitchgearTechFooter from "./SwitchgearTechFooter.vue"
import SwitchgearMenu from "./SwitchgearMenu.vue"

const props = withDefaults(defineProps<{
  title?: string
  doUnitId?: string
  doOpenCh?: number
  doCloseCh?: number
  diUnitId?: string
  diOpenPulseCh?: number
  diClosePulseCh?: number
  selected?: boolean
}>(), {
  title: "2-Pos Switchgear",
  doOpenCh: 0,
  doCloseCh: 1,
  diOpenPulseCh: 0,
  diClosePulseCh: 1,
  selected: false,
})

const emit = defineEmits<{
  (e: "delete"): void
}>()

const {
  effectiveState, pendingTarget, busy, feedbackDelayMs,
  doUnitId, diUnitId,
  isCmdDisabled, setDoPair,
} = useSwitchgear({
  doUnitId: props.doUnitId,
  doOpenCh: props.doOpenCh,
  doCloseCh: props.doCloseCh,
  diUnitId: props.diUnitId,
  diOpenPulseCh: props.diOpenPulseCh,
  diClosePulseCh: props.diClosePulseCh,
})

// Style helpers for state pill
const statePillClass = computed(() => {
  switch (effectiveState.value) {
    case "CLOSED": return "bg-neutral-900 text-white dark:bg-neutral-100 dark:text-neutral-900"
    case "OPEN": return "bg-neutral-100 text-neutral-700 dark:bg-neutral-800 dark:text-neutral-200"
    case "UNKNOWN": return "bg-amber-100 text-amber-800 dark:bg-amber-900/40 dark:text-amber-300"
    case "INTERMEDIATE": return "bg-sky-100 text-sky-800 dark:bg-sky-900/40 dark:text-sky-300"
  }
})
</script>
