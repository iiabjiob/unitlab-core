<script setup lang="ts">
import { computed, getCurrentInstance, onBeforeUnmount, watch } from "vue"
import type { ComponentPublicInstance } from "vue"
import { useFloatingTooltip, useTooltipController, type TooltipController } from "@affino/tooltip-vue"
import {
  activateTooltipController,
  deactivateTooltipController,
  registerTooltipController,
  unregisterTooltipController,
} from "@/components/ui/tooltipSingletonRegistry"
import {
  DEFAULT_TOOLTIP_CLOSE_DELAY_MS,
  DEFAULT_TOOLTIP_OPEN_DELAY_MS,
} from "@/components/ui/tooltipDefaults"

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
    openDelay: DEFAULT_TOOLTIP_OPEN_DELAY_MS,
    closeDelay: DEFAULT_TOOLTIP_CLOSE_DELAY_MS,
  },
)

const instanceUid = getCurrentInstance()?.uid ?? Math.floor(Math.random() * 1_000_000)
const tooltipController = useTooltipController({
  id: `ui-hover-tooltip-${instanceUid}`,
  openDelay: props.openDelay,
  closeDelay: props.closeDelay,
})

registerTooltipController(tooltipController as TooltipController)

const { triggerRef, tooltipRef, tooltipStyle, teleportTarget } = useFloatingTooltip(tooltipController, {
  placement: props.placement,
  align: props.align,
  gutter: 8,
})

function getTriggerProps() {
  if (props.disabled) {
    return {}
  }
  return tooltipController.getTriggerProps()
}

const tooltipProps = computed(() => tooltipController.getTooltipProps())

watch(
  () => tooltipController.state.value.open,
  (isOpen) => {
    if (!isOpen) {
      deactivateTooltipController(tooltipController.id)
      return
    }
    activateTooltipController(tooltipController.id)
  },
)

watch(
  () => props.disabled,
  (disabled) => {
    if (!disabled || !tooltipController.state.value.open) {
      return
    }
    tooltipController.close("programmatic")
  },
)

onBeforeUnmount(() => {
  unregisterTooltipController(tooltipController.id)
})

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
  <slot :setTriggerRef="setTriggerRef" :getTriggerProps="getTriggerProps" />

  <Teleport :to="teleportTarget || 'body'">
    <div
      v-if="!disabled && tooltipController.state.value.open"
      ref="tooltipRef"
      class="ui-hover-tooltip z-50 w-max max-w-xs whitespace-nowrap rounded-md border border-neutral-300 bg-white px-2.5 py-1.5 text-xs leading-5 text-neutral-700 shadow-sm dark:border-neutral-700 dark:bg-neutral-900 dark:text-neutral-200"
      v-bind="tooltipProps"
      :style="tooltipStyle"
    >
      {{ text }}
    </div>
  </Teleport>
</template>
