<template>
  <div class="external-ied-ip-cell" :title="title">
    <span class="external-ied-ip-cell__label-wrap">
      <span
        v-if="status !== 'not_applicable'"
        class="external-ied-ip-cell__indicator"
        :class="indicatorClass"
        aria-hidden="true"
      ></span>
      <span class="external-ied-ip-cell__label" :class="labelClass">{{ label }}</span>
    </span>
    <span
      v-if="detailsEnabled"
      class="external-ied-ip-cell__details-affordance"
      role="button"
      tabindex="0"
      :aria-label="detailsLabel"
      @click.stop="emit('openDetails')"
      @keydown.enter.stop.prevent="emit('openDetails')"
      @keydown.space.stop.prevent="emit('openDetails')"
    >›</span>
  </div>
</template>

<script setup lang="ts">
import { computed } from "vue"
import type { ExternalIedStatus } from "@/stores/externalIedStore"

const props = defineProps<{
  label: string
  status: ExternalIedStatus
  detailsEnabled?: boolean
  detailsLabel?: string
}>()

const emit = defineEmits<{ (event: "openDetails"): void }>()

const title = computed(() => {
  if (props.status === "reachable") return `${props.label} · MMS reachable`
  if (props.status === "offline") return `${props.label} · MMS unavailable`
  if (props.status === "expected") return `${props.label} · Expected`
  if (props.status === "unknown") return `${props.label} · Checking`
  return props.label
})

const indicatorClass = computed(() => {
  if (props.status === "reachable") return "external-ied-ip-cell__indicator--reachable"
  if (props.status === "offline") return "external-ied-ip-cell__indicator--offline"
  return "external-ied-ip-cell__indicator--unknown"
})

const labelClass = computed(() => {
  if (props.status === "reachable") return "external-ied-ip-cell__label--reachable"
  if (props.status === "offline") return "external-ied-ip-cell__label--offline"
  return "external-ied-ip-cell__label--unknown"
})
</script>

<style scoped>
.external-ied-ip-cell {
  align-items: center;
  display: flex;
  gap: 0.25rem;
  justify-content: space-between;
  min-width: 0;
  width: 100%;
}

.external-ied-ip-cell__label-wrap {
  align-items: center;
  display: flex;
  gap: 0.375rem;
  min-width: 0;
}

.external-ied-ip-cell__details-affordance {
  align-items: center;
  border-radius: var(--radius-pill);
  color: var(--color-neutral-500);
  cursor: pointer;
  display: inline-flex;
  flex: 0 0 1.25rem;
  font-size: var(--text-sm);
  height: 1.25rem;
  justify-content: center;
  opacity: 0;
  transform: translateX(0.125rem);
  transition: opacity 120ms ease, transform 120ms ease, background 120ms ease, color 120ms ease;
}

.external-ied-ip-cell:hover .external-ied-ip-cell__details-affordance,
.external-ied-ip-cell__details-affordance:focus-visible {
  opacity: 1;
  transform: translateX(0);
}

.external-ied-ip-cell__details-affordance:hover,
.external-ied-ip-cell__details-affordance:focus-visible {
  background: var(--color-neutral-100);
  color: var(--color-neutral-900);
  outline: none;
}

.external-ied-ip-cell__indicator {
  border-radius: var(--radius-pill);
  flex-shrink: 0;
  height: 0.375rem;
  width: 0.375rem;
}

.external-ied-ip-cell__indicator--reachable {
  background: var(--color-emerald-500);
}

.external-ied-ip-cell__indicator--offline {
  background: var(--color-neutral-400);
}

.external-ied-ip-cell__indicator--unknown {
  background: var(--color-sky-500);
}

.external-ied-ip-cell__label {
  color: var(--color-neutral-700);
  font-size: var(--text-xs);
  min-width: 0;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.external-ied-ip-cell__label--reachable {
  color: var(--color-neutral-900);
}

.external-ied-ip-cell__label--offline,
.external-ied-ip-cell__label--unknown {
  color: var(--color-neutral-700);
}

:global(.dark .external-ied-ip-cell__indicator--offline) {
  background: var(--color-neutral-600);
}

:global(.dark .external-ied-ip-cell__label--reachable) {
  color: var(--color-neutral-100);
}

:global(.dark .external-ied-ip-cell__label--offline),
:global(.dark .external-ied-ip-cell__label--unknown) {
  color: var(--color-neutral-300);
}

:global(.dark .external-ied-ip-cell__details-affordance:hover),
:global(.dark .external-ied-ip-cell__details-affordance:focus-visible) {
  background: var(--color-neutral-800);
  color: var(--color-neutral-100);
}
</style>
