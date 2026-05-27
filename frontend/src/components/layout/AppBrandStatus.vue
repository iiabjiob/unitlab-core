<template>
  <div class="app-brand-status">
    <AppLogo />
    <div
      class="app-brand-status__meta"
      :class="systemStatusClass"
      :title="systemStatusTitle"
    >
      <span class="app-brand-status__system">
        <span class="app-brand-status__system-dot" aria-hidden="true" />
        <span>system:<span class="app-brand-status__system-value">{{ systemStatusLabel }}</span></span>
      </span>
      <span class="app-brand-status__separator" aria-hidden="true">|</span>
      <TimeComponent
        class="app-brand-status__clock"
        variant="inline"
        time-label="time:"
      />
    </div>
  </div>
</template>

<script setup lang="ts">
import { computed } from "vue"

import AppLogo from "./AppLogo.vue"
import TimeComponent from "../misc/TimeComponent.vue"
import { useSystemHealthStore } from "@/stores/systemHealthStore"

const systemHealthStore = useSystemHealthStore()

const status = computed(() => systemHealthStore.status)
const statusDescription = computed(() => (
  status.value === "degraded" ? systemHealthStore.tooltip : null
))
const systemStatusLabel = computed(() => String(status.value ?? "n/a").trim() || "n/a")
const systemStatusTitle = computed(() => (
  statusDescription.value
    ? `System status: ${systemStatusLabel.value}. ${statusDescription.value}`
    : `System status: ${systemStatusLabel.value}`
))
const systemStatusClass = computed(() => {
  if (status.value === "online") return "is-online"
  if (status.value === "degraded") return "is-degraded"
  return "is-offline"
})
</script>

<style scoped>
.app-brand-status {
  display: inline-flex;
  align-items: center;
  gap: 0.875rem;
  min-width: 0;
}

.app-brand-status__meta {
  display: inline-flex;
  align-items: center;
  gap: 0.375rem;
  min-width: 0;
  overflow: hidden;
  color: var(--color-neutral-600);
  font-size: var(--text-xs);
  line-height: 1;
  white-space: nowrap;
}

.app-brand-status__system {
  display: inline-flex;
  align-items: center;
  gap: 0.25rem;
  min-width: 0;
}

.app-brand-status__system-dot {
  width: 0.5rem;
  height: 0.5rem;
  flex: 0 0 auto;
  border: 1px solid color-mix(in srgb, var(--color-white) 70%, transparent);
  border-radius: 999px;
  box-shadow: var(--shadow-sm);
}

.app-brand-status__system-value {
  font-weight: 600;
}

.app-brand-status__meta.is-online .app-brand-status__system-dot {
  background: var(--color-green-400);
}

.app-brand-status__meta.is-online .app-brand-status__system-value {
  color: var(--color-green-600);
}

.app-brand-status__meta.is-degraded .app-brand-status__system-dot {
  background: var(--color-amber-400);
}

.app-brand-status__meta.is-degraded .app-brand-status__system-value {
  color: var(--color-amber-600);
}

.app-brand-status__meta.is-offline .app-brand-status__system-dot {
  background: var(--color-neutral-400);
}

.app-brand-status__meta.is-offline .app-brand-status__system-value {
  color: var(--color-neutral-500);
}

.app-brand-status__separator {
  color: var(--color-neutral-400);
}

:global(.dark .app-brand-status__meta) {
  color: var(--color-neutral-400);
}

:global(.dark .app-brand-status__meta.is-online .app-brand-status__system-value) {
  color: var(--color-green-400);
}

:global(.dark .app-brand-status__meta.is-degraded .app-brand-status__system-value) {
  color: var(--color-amber-400);
}

:global(.dark .app-brand-status__meta.is-offline .app-brand-status__system-value) {
  color: var(--color-neutral-400);
}

:global(.dark .app-brand-status__separator) {
  color: var(--color-neutral-600);
}
</style>
