<script setup lang="ts">
import { computed } from "vue"
import { useFloatingTooltip, useTooltipController } from "@affino/tooltip-vue"

const props = defineProps<{
  status: "online" | "offline" | "degraded" | string
  description?: string | null
  neutralOffline?: boolean
}>()

const indicatorClass = computed(() => {
  switch (props.status) {
    case "online":
      return "bg-green-400"
    case "degraded":
      return "bg-amber-400"
    case "offline":
      return props.neutralOffline ? "bg-neutral-400" : "bg-rose-500"
    default:
      return "bg-neutral-400"
  }
})

const labelClass = computed(() => {
  switch (props.status) {
    case "online":
      return "text-green-500"
    case "degraded":
      return "text-amber-500"
    case "offline":
      return props.neutralOffline ? "text-neutral-500" : "text-rose-500"
    default:
      return "text-neutral-500"
  }
})

const label = computed(() => {
  if (props.status === "online") return "online"
  if (props.status === "offline") return "offline"
  if (props.status === "degraded") return "degraded"
  return props.status ?? "n/a"
})

const tooltip = computed(() => props.description || null)
const tooltipController = useTooltipController({
  id: "system-status-tooltip",
  openDelay: 120,
})
const { triggerRef, tooltipRef, tooltipStyle, teleportTarget } = useFloatingTooltip(tooltipController, {
  placement: "bottom",
  align: "center",
  gutter: 8,
})
const triggerProps = computed(() =>
  tooltip.value
    ? tooltipController.getTriggerProps()
    : {
        "aria-label": `System status: ${label.value}`,
      },
)
</script>

<template>
  <span ref="triggerRef" class="inline-flex items-center gap-2" v-bind="triggerProps">
    <span class="h-2.5 w-2.5 rounded-full border border-white/70 shadow-sm" :class="indicatorClass" />
    <span class="text-sm font-medium" :class="labelClass">
      {{ label }}
    </span>
    <span v-if="tooltip" class="text-xs text-neutral-400 hidden sm:inline">ⓘ</span>
  </span>

  <Teleport :to="teleportTarget || 'body'">
    <div
      v-if="tooltip && tooltipController.state.value.open"
      ref="tooltipRef"
      class="pointer-events-none z-50 w-max max-w-xs rounded-md border border-neutral-300 bg-white px-2.5 py-1.5 text-xs text-neutral-700 shadow-sm dark:border-neutral-700 dark:bg-neutral-900 dark:text-neutral-200"
      v-bind="tooltipController.getTooltipProps()"
      :style="tooltipStyle"
    >
      <span class="whitespace-pre-line">{{ tooltip }}</span>
    </div>
  </Teleport>
</template>
