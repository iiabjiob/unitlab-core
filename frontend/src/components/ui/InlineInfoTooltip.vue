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
    class="inline-info-tooltip__trigger"
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
      class="inline-info-tooltip__content"
      v-bind="tooltipController.getTooltipProps()"
      :style="tooltipStyle"
    >
      {{ text }}
    </div>
  </Teleport>
</template>

<style scoped>
.inline-info-tooltip__trigger {
  align-items: center;
  border-radius: 999px;
  color: color-mix(in srgb, var(--color-neutral-400) 80%, transparent);
  display: inline-flex;
  font-size: 0.625rem;
  font-weight: 500;
  height: 1rem;
  justify-content: center;
  line-height: 1;
  transition: color 150ms ease, box-shadow 150ms ease;
  cursor: default !important;
  user-select: none;
  width: 1rem;
}

.inline-info-tooltip__trigger:hover {
  color: var(--color-neutral-500);
}

.inline-info-tooltip__trigger:focus-visible {
  box-shadow: 0 0 0 1px color-mix(in srgb, var(--color-blue-500) 40%, transparent);
  outline: none;
}

.inline-info-tooltip__content {
  background: var(--color-white);
  border: 1px solid var(--color-neutral-300);
  border-radius: var(--radius-md);
  box-shadow: var(--shadow-sm);
  color: var(--color-neutral-700);
  font-size: var(--text-xs);
  line-height: 1.25rem;
  max-width: 20rem;
  padding: 0.375rem 0.625rem;
  white-space: pre-line;
  width: max-content;
  z-index: 50;
}

:global(.dark .inline-info-tooltip__trigger) {
  color: var(--color-neutral-500);
}

:global(.dark .inline-info-tooltip__trigger:hover) {
  color: var(--color-neutral-300);
}

:global(.dark .inline-info-tooltip__content) {
  background: var(--color-neutral-900);
  border-color: var(--color-neutral-700);
  color: var(--color-neutral-200);
}
</style>
