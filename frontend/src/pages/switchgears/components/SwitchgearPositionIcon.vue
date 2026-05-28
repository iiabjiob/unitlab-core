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
      return "Open"
    case "CLOSED":
      return "Closed"
    case "INTERMEDIATE":
      return "Undefined"
    default:
      return "Unknown"
  }
})

const sizeClass = computed(() => {
  switch (props.size) {
    case "lg":
      return "switchgear-position-icon__graphic--lg"
    case "md":
      return "switchgear-position-icon__graphic--md"
    default:
      return "switchgear-position-icon__graphic--sm"
  }
})
</script>

<template>
  <span class="switchgear-position-icon" role="img" :aria-label="`Position: ${label}`" :title="label">
    <svg
      viewBox="0 0 16 16"
      fill="none"
      class="switchgear-position-icon__graphic"
      :class="sizeClass"
    >
      <rect
        x="2.5"
        y="2.5"
        width="11"
        height="11"
        rx="2"
        class="switchgear-position-icon__plate"
      />

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

<style scoped>
.switchgear-position-icon {
  display: inline-flex;
  align-items: center;
}

.switchgear-position-icon__graphic {
  display: block;
  color: var(--color-blue-600);
  filter: drop-shadow(0 1px 1px rgb(0 0 0 / 15%));
}

.switchgear-position-icon__plate {
  fill: var(--switchgear-position-icon-plate, transparent);
}

.switchgear-position-icon__graphic--sm {
  width: 1.5rem;
  height: 1.5rem;
}

.switchgear-position-icon__graphic--md {
  width: 1.75rem;
  height: 1.75rem;
}

.switchgear-position-icon__graphic--lg {
  width: 3rem;
  height: 3rem;
}

:global(.dark .switchgear-position-icon__graphic) {
  color: var(--color-blue-400);
}
</style>
