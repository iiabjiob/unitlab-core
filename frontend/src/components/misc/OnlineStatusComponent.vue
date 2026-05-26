<script setup lang="ts">
import { computed } from "vue"
import InlineInfoTooltip from "@/components/ui/InlineInfoTooltip.vue"

const props = defineProps<{
  status: "online" | "offline" | "degraded" | string
  description?: string | null
  neutralOffline?: boolean
}>()

const statusClass = computed(() => {
  switch (props.status) {
    case "online":
      return "is-online"
    case "degraded":
      return "is-degraded"
    case "offline":
      return "is-offline"
    default:
      return "is-unknown"
  }
})

const label = computed(() => {
  if (props.status === "online") return "online"
  if (props.status === "offline") return "offline"
  if (props.status === "degraded") return "degraded"
  return props.status ?? "n/a"
})

const tooltip = computed(() => props.description || null)
</script>

<template>
  <span class="online-status" :class="statusClass" :aria-label="`System status: ${label}`">
    <span class="online-status__indicator" />
    <span class="online-status__label">
      {{ label }}
    </span>
    <InlineInfoTooltip
      v-if="tooltip"
      class="online-status__tooltip"
      :text="tooltip"
      aria-label="System status details"
      placement="bottom"
      align="start"
    />
  </span>
</template>

<style scoped>
.online-status {
  align-items: center;
  display: inline-flex;
  gap: 0.5rem;
}

.online-status__indicator {
  border: 1px solid color-mix(in srgb, var(--color-white) 70%, transparent);
  border-radius: 999px;
  box-shadow: var(--shadow-sm);
  height: 0.625rem;
  width: 0.625rem;
}

.online-status__label {
  font-size: var(--text-sm);
  font-weight: 500;
  line-height: 1.25rem;
}

.online-status.is-online .online-status__indicator {
  background: var(--color-green-400);
}

.online-status.is-online .online-status__label {
  color: var(--color-green-600);
}

.online-status.is-degraded .online-status__indicator {
  background: var(--color-amber-400);
}

.online-status.is-degraded .online-status__label {
  color: var(--color-amber-500);
}

.online-status.is-offline .online-status__indicator,
.online-status.is-unknown .online-status__indicator {
  background: var(--color-neutral-400);
}

.online-status.is-offline .online-status__label,
.online-status.is-unknown .online-status__label {
  color: var(--color-neutral-500);
}

.online-status__tooltip {
  display: none;
}

@media (min-width: 640px) {
  .online-status__tooltip {
    display: inline-flex;
  }
}
</style>
