<template>
  <span class="external-ied-address-cell" :class="rootClass" :title="title">
    <span v-if="coverageStatus !== 'not_planned'" class="external-ied-address-cell__indicator" aria-hidden="true"></span>
    <span class="external-ied-address-cell__label">{{ label }}</span>
  </span>
</template>

<script setup lang="ts">
import { computed } from "vue"
import type { ExternalIedPlanningSignalStatus } from "@/stores/externalIedStore"

const props = defineProps<{
  label: string
  coverageStatus: ExternalIedPlanningSignalStatus | "not_planned"
  reason?: string | null
}>()

const rootClass = computed(() => `external-ied-address-cell--${props.coverageStatus.replace("_", "-")}`)

const title = computed(() => {
  if (props.coverageStatus === "matched") return `${props.label} · matched to discovered IED model`
  if (props.coverageStatus === "unmatched") return `${props.label} · not found in discovered IED model${props.reason ? ` · ${props.reason}` : ""}`
  if (props.coverageStatus === "ambiguous") return `${props.label} · ambiguous match${props.reason ? ` · ${props.reason}` : ""}`
  if (props.coverageStatus === "stale") return `${props.label} · planning stale`
  return props.label
})
</script>

<style scoped>
.external-ied-address-cell {
  align-items: center;
  display: inline-flex;
  gap: 0.375rem;
  max-width: 100%;
  min-width: 0;
}

.external-ied-address-cell__indicator {
  border-radius: 999px;
  flex: 0 0 0.375rem;
  height: 0.375rem;
  width: 0.375rem;
}

.external-ied-address-cell__label {
  font-size: var(--text-xs);
  min-width: 0;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.external-ied-address-cell--matched {
  color: var(--color-emerald-700);
}

.external-ied-address-cell--matched .external-ied-address-cell__indicator {
  background: var(--color-emerald-500);
}

.external-ied-address-cell--unmatched {
  color: var(--color-rose-700);
}

.external-ied-address-cell--unmatched .external-ied-address-cell__indicator {
  background: var(--color-rose-500);
}

.external-ied-address-cell--ambiguous {
  color: var(--color-amber-700);
}

.external-ied-address-cell--ambiguous .external-ied-address-cell__indicator {
  background: var(--color-amber-400);
}

.external-ied-address-cell--stale {
  color: var(--color-neutral-600);
}

.external-ied-address-cell--stale .external-ied-address-cell__indicator {
  background: var(--color-neutral-400);
}

.external-ied-address-cell--not-planned {
  color: var(--color-neutral-700);
}

:global(.dark .external-ied-address-cell--matched) {
  color: var(--color-emerald-300);
}

:global(.dark .external-ied-address-cell--unmatched) {
  color: var(--color-rose-300);
}

:global(.dark .external-ied-address-cell--ambiguous) {
  color: var(--color-amber-300);
}

:global(.dark .external-ied-address-cell--stale),
:global(.dark .external-ied-address-cell--not-planned) {
  color: var(--color-neutral-300);
}
</style>
