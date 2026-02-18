<script setup lang="ts">
import { computed } from "vue"
import InlineInfoTooltip from "@/components/ui/InlineInfoTooltip.vue"

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
      return "bg-neutral-400"
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
      return "text-neutral-500"
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
</script>

<template>
  <span class="inline-flex items-center gap-2" :aria-label="`System status: ${label}`">
    <span class="h-2.5 w-2.5 rounded-full border border-white/70 shadow-sm" :class="indicatorClass" />
    <span class="text-sm font-medium" :class="labelClass">
      {{ label }}
    </span>
    <InlineInfoTooltip
      v-if="tooltip"
      class="hidden sm:inline-flex"
      :text="tooltip"
      aria-label="System status details"
      placement="bottom"
      align="start"
    />
  </span>
</template>
