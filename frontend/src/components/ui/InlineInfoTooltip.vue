<script setup lang="ts">
import { computed, getCurrentInstance } from "vue"
import { useFloatingTooltip, useTooltipController } from "@affino/tooltip-vue"

type TooltipPlacement = "top" | "bottom" | "left" | "right"
type TooltipAlign = "start" | "center" | "end"

const props = withDefaults(
  defineProps<{
    text: string
    ariaLabel?: string
    placement?: TooltipPlacement
    align?: TooltipAlign
  }>(),
  {
    ariaLabel: "Field explanation",
    placement: "top",
    align: "start",
  },
)

const instanceUid = getCurrentInstance()?.uid ?? Math.floor(Math.random() * 1_000_000)
const tooltipController = useTooltipController({
  id: `inline-info-tooltip-${instanceUid}`,
  openDelay: 120,
})

const { triggerRef, tooltipRef, tooltipStyle, teleportTarget } = useFloatingTooltip(tooltipController, {
  placement: props.placement,
  align: props.align,
  gutter: 8,
})

const triggerProps = computed(() => tooltipController.getTriggerProps())
</script>

<template>
  <span
    ref="triggerRef"
    class="inline-flex h-4 w-4 select-none items-center justify-center rounded-full text-[10px] font-medium leading-none text-neutral-400/80 transition-colors hover:text-neutral-500 focus-visible:outline-none focus-visible:ring-1 focus-visible:ring-blue-500/40 dark:text-neutral-500 dark:hover:text-neutral-300"
    role="button"
    tabindex="0"
    :aria-label="ariaLabel"
    v-bind="triggerProps"
  >
    ⓘ
  </span>

  <Teleport :to="teleportTarget || 'body'">
    <div
      v-if="tooltipController.state.value.open"
      ref="tooltipRef"
      class="pointer-events-none z-50 w-max max-w-xs whitespace-pre-line rounded-md border border-neutral-300 bg-white px-2.5 py-1.5 text-xs leading-5 text-neutral-700 shadow-sm dark:border-neutral-700 dark:bg-neutral-900 dark:text-neutral-200"
      v-bind="tooltipController.getTooltipProps()"
      :style="tooltipStyle"
    >
      {{ text }}
    </div>
  </Teleport>
</template>
