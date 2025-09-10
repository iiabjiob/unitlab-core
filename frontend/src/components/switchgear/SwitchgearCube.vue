<!-- src/components/switchgear/SwitchgearCube.vue -->
<template>
  <div
    class="h-28 w-28 mx-auto rounded-lg transition-all duration-200 border-2 flex items-center justify-center select-none relative"
    :class="cubeClass"
    :title="`State (by DO): ${effectiveState}`"
  >
    <template v-if="effectiveState === 'UNKNOWN'">
      <span class="text-3xl font-bold">?</span>
    </template>
    <template v-else-if="effectiveState === 'INTERMEDIATE'">
      <div class="relative w-full h-full flex items-center justify-center">
        <div class="absolute rotate-45 w-[140%] h-[3px] bg-current opacity-80"></div>
      </div>
    </template>

    <div v-if="pendingTarget" class="pointer-events-none absolute inset-0 rounded-lg border-2 animate-pulse" :class="pendingBorderClass"/>
  </div>
</template>

<script setup lang="ts">
import { computed } from "vue"
import type { SwitchgearState } from "@/composables/useSwitchgear"

const props = defineProps<{
  effectiveState: SwitchgearState
  pendingTarget: SwitchgearState | null
}>()

const cubeClass = computed(() => {
  switch (props.effectiveState) {
    case "CLOSED":
      return "bg-neutral-900 dark:bg-neutral-100 text-white dark:text-neutral-900 border-neutral-900 dark:border-neutral-100"
    case "OPEN":
      return "bg-transparent text-neutral-700 dark:text-neutral-200 border-neutral-700 dark:border-neutral-300"
    case "UNKNOWN":
      return "bg-transparent text-amber-600 dark:text-amber-400 border-amber-600/70 dark:border-amber-400/70"
    case "INTERMEDIATE":
      return "bg-transparent text-sky-600 dark:text-sky-400 border-sky-600/70 dark:border-sky-400/70"
  }
})

const pendingBorderClass = computed(() => {
  switch (props.pendingTarget) {
    case "OPEN": return "border-sky-500/60"
    case "CLOSED": return "border-neutral-900/60 dark:border-neutral-100/60"
    case "UNKNOWN": return "border-amber-500/60"
    case "INTERMEDIATE": return "border-sky-500/60"
    default: return "border-transparent"
  }
})
</script>
