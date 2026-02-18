<script setup lang="ts">
import { computed, getCurrentInstance } from "vue"
import type { ComponentPublicInstance } from "vue"
import { useFloatingTooltip, useTooltipController } from "@affino/tooltip-vue"

type TooltipPlacement = "top" | "bottom" | "left" | "right"
type TooltipAlign = "start" | "center" | "end"

const props = withDefaults(
  defineProps<{
    text: string
    ariaLabel?: string
    placement?: TooltipPlacement
    align?: TooltipAlign
    disabled?: boolean
    openDelay?: number
    closeDelay?: number
  }>(),
  {
    ariaLabel: "Tooltip",
    placement: "right",
    align: "center",
    disabled: false,
    openDelay: 120,
    closeDelay: 120,
  },
)

const instanceUid = getCurrentInstance()?.uid ?? Math.floor(Math.random() * 1_000_000)
const tooltipController = useTooltipController({
  id: `ui-hover-tooltip-${instanceUid}`,
  openDelay: props.openDelay,
  closeDelay: props.closeDelay,
})

const { triggerRef, tooltipRef, tooltipStyle, teleportTarget } = useFloatingTooltip(tooltipController, {
  placement: props.placement,
  align: props.align,
  gutter: 8,
})

const triggerProps = computed(() => (
  props.disabled
    ? {}
    : tooltipController.getTriggerProps()
))

const tooltipProps = computed(() => tooltipController.getTooltipProps())

function setTriggerRef(target: Element | ComponentPublicInstance | null) {
  if (target instanceof HTMLElement) {
    triggerRef.value = target
    return
  }
  if (target && "$el" in target && target.$el instanceof HTMLElement) {
    triggerRef.value = target.$el
    return
  }
  triggerRef.value = null
}
</script>

<template>
  <slot :set-trigger-ref="setTriggerRef" :trigger-props="triggerProps" />

  <Teleport :to="teleportTarget || 'body'">
    <div
      v-if="!disabled"
      ref="tooltipRef"
      class="ui-hover-tooltip pointer-events-none z-50 w-max max-w-xs whitespace-nowrap rounded-md border border-neutral-300 bg-white px-2.5 py-1.5 text-xs leading-5 text-neutral-700 shadow-sm dark:border-neutral-700 dark:bg-neutral-900 dark:text-neutral-200"
      v-bind="tooltipProps"
      :style="tooltipStyle"
    >
      {{ text }}
    </div>
  </Teleport>
</template>

<style scoped>
.ui-hover-tooltip[data-state="closed"] {
  display: none;
}
</style>
