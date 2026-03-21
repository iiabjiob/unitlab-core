<script setup lang="ts">
import { computed, getCurrentInstance, onBeforeUnmount, useAttrs, watch } from "vue"
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
import { APP_OVERLAY_HOST_SELECTOR } from "@/utils/overlayHost"

defineOptions({
  inheritAttrs: false,
})

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
    ariaLabel: "Field explanation",
    placement: "top",
    align: "start",
    disabled: false,
    openDelay: DEFAULT_TOOLTIP_OPEN_DELAY_MS,
    closeDelay: DEFAULT_TOOLTIP_CLOSE_DELAY_MS,
  },
)

const instanceUid = getCurrentInstance()?.uid ?? Math.floor(Math.random() * 1_000_000)
const attrs = useAttrs()
const tooltipController = useTooltipController({
  id: `inline-info-tooltip-${instanceUid}`,
  openDelay: props.openDelay,
  closeDelay: props.closeDelay,
})

const { triggerRef, tooltipRef, tooltipStyle, teleportTarget } = useFloatingTooltip(tooltipController, {
  placement: props.placement,
  align: props.align,
  gutter: 8,
  teleportTo: APP_OVERLAY_HOST_SELECTOR,
})

registerTooltipController(tooltipController as TooltipController)

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

function getTriggerProps() {
  if (props.disabled) {
    return {}
  }
  return tooltipController.getTriggerProps()
}

const triggerAttrs = computed(() => ({
  ...attrs,
  ...getTriggerProps(),
}))

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
  <slot v-if="$slots.default" :setTriggerRef="setTriggerRef" :getTriggerProps="getTriggerProps" />

  <span
    v-else
    ref="triggerRef"
    class="inline-flex h-4 w-4 select-none items-center justify-center rounded-full text-[10px] font-medium leading-none text-neutral-400/80 transition-colors hover:text-neutral-500 focus-visible:outline-none focus-visible:ring-1 focus-visible:ring-blue-500/40 dark:text-neutral-500 dark:hover:text-neutral-300"
    v-bind="triggerAttrs"
    role="button"
    tabindex="0"
    :aria-label="ariaLabel"
  >
    ⓘ
  </span>

  <Teleport :to="teleportTarget || APP_OVERLAY_HOST_SELECTOR">
    <div
      v-if="!disabled && tooltipController.state.value.open"
      ref="tooltipRef"
      class="z-50 w-max max-w-xs whitespace-pre-line rounded-md border border-neutral-300 bg-white px-2.5 py-1.5 text-xs leading-5 text-neutral-700 shadow-sm dark:border-neutral-700 dark:bg-neutral-900 dark:text-neutral-200"
      v-bind="tooltipController.getTooltipProps()"
      :style="tooltipStyle"
    >
      {{ text }}
    </div>
  </Teleport>
</template>
