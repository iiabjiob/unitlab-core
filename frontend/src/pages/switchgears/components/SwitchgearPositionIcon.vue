<script setup lang="ts">
import { computed } from "vue"
import type { SwitchgearState } from "@/constants/switchgear"

const props = withDefaults(defineProps<{
  state: SwitchgearState
  size?: "sm" | "md" | "lg"
}>(), {
  size: "sm",
})

const label = computed(() => {
  switch (props.state) {
    case "OPEN":
      return "Off"
    case "CLOSED":
      return "On"
    case "INTERMEDIATE":
      return "Intermediate"
    default:
      return "Unknown"
  }
})

const sizeClass = computed(() => {
  switch (props.size) {
    case "lg":
      return "w-12 h-12"
    case "md":
      return "w-7 h-7"
    default:
      return "w-6 h-6"
  }
})

const colorClass = computed(() => {
  return "text-blue-600 dark:text-blue-400"
})
</script>

<template>
  <span class="inline-flex items-center" role="img" :aria-label="`Position: ${label}`" :title="label">
    <svg
      viewBox="0 0 16 16"
      fill="none"
      :class="['block drop-shadow-sm', sizeClass, colorClass]"
    >
      <rect
        v-if="state === 'CLOSED'"
        x="2.5"
        y="2.5"
        width="11"
        height="11"
        rx="2"
        fill="currentColor"
      />
      <rect
        v-else
        x="2.5"
        y="2.5"
        width="11"
        height="11"
        rx="2"
        stroke="currentColor"
        stroke-width="1.6"
      />

      <line
        v-if="state === 'INTERMEDIATE'"
        x1="4"
        y1="12"
        x2="12"
        y2="4"
        stroke="currentColor"
        stroke-width="1.6"
        stroke-linecap="round"
      />

      <text
        v-else-if="state === 'UNKNOWN'"
        x="8"
        y="8.4"
        text-anchor="middle"
        dominant-baseline="middle"
        font-size="9"
        font-weight="700"
        fill="currentColor"
      >
        ?
      </text>
    </svg>
  </span>
</template>
