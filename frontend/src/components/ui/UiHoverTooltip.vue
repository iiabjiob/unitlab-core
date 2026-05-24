<script setup lang="ts">
import { computed, getCurrentInstance, onBeforeUnmount, ref, watch } from "vue"
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
    multiline?: boolean
    zIndex?: number | string
    openOnFocus?: boolean
    suppressOpenOnClick?: boolean
    strictTriggerHover?: boolean
  }>(),
  {
    ariaLabel: "Tooltip",
    placement: "right",
    align: "center",
    disabled: false,
    openDelay: DEFAULT_TOOLTIP_OPEN_DELAY_MS,
    closeDelay: DEFAULT_TOOLTIP_CLOSE_DELAY_MS,
    multiline: false,
    zIndex: undefined,
    openOnFocus: true,
    suppressOpenOnClick: false,
    strictTriggerHover: false,
  },
)

const instanceUid = getCurrentInstance()?.uid ?? Math.floor(Math.random() * 1_000_000)
const suppressOpenUntil = ref(0)
const triggerPointerInside = ref(false)
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
  teleportTo: APP_OVERLAY_HOST_SELECTOR,
  zIndex: props.zIndex,
})

function getTriggerProps() {
  if (props.disabled) {
    return {}
  }
  const triggerProps = tooltipController.getTriggerProps()
  const onPointerenter = (event: Parameters<NonNullable<typeof triggerProps.onPointerenter>>[0]) => {
    triggerPointerInside.value = true
    if (props.suppressOpenOnClick && Date.now() < suppressOpenUntil.value) {
      return
    }
    triggerProps.onPointerenter?.(event)
  }
  const onPointerleave = (event: Parameters<NonNullable<typeof triggerProps.onPointerleave>>[0]) => {
    triggerPointerInside.value = false
    triggerProps.onPointerleave?.(event)
    if (props.strictTriggerHover && tooltipController.state.value.open) {
      tooltipController.close("pointer")
    }
  }
  const resolvedTriggerProps = props.openOnFocus
    ? {
        ...triggerProps,
        onPointerenter,
        onPointerleave,
      }
    : {
        id: triggerProps.id,
        "aria-describedby": triggerProps["aria-describedby"],
        onPointerenter,
        onPointerleave,
      }

  if (!props.suppressOpenOnClick) {
    return resolvedTriggerProps
  }

  return {
    ...resolvedTriggerProps,
    onPointerdown: suppressClickOpen,
    onClick: suppressClickOpen,
  }
}

const tooltipProps = computed(() => tooltipController.getTooltipProps())
const tooltipClass = computed(() => [
  "ui-hover-tooltip z-50 w-max rounded-md border border-neutral-300 bg-white px-2.5 py-1.5 text-xs leading-5 text-neutral-700 shadow-sm dark:border-neutral-700 dark:bg-neutral-900 dark:text-neutral-200",
  props.multiline ? "max-w-sm whitespace-pre-line" : "max-w-xs whitespace-nowrap",
])

function suppressClickOpen() {
  suppressOpenUntil.value = Date.now() + Math.max(0, props.openDelay) + 80
  if (tooltipController.state.value.open) {
    tooltipController.close("programmatic")
  }
}

function shouldForceCloseOpenTooltip(): boolean {
  return (
    (props.suppressOpenOnClick && Date.now() < suppressOpenUntil.value)
    || (props.strictTriggerHover && !triggerPointerInside.value)
  )
}

watch(
  () => tooltipController.state.value.open,
  (isOpen) => {
    if (!isOpen) {
      deactivateTooltipController(tooltipController.id)
      return
    }
    if (shouldForceCloseOpenTooltip()) {
      tooltipController.close("programmatic")
      return
    }
    activateTooltipController(tooltipController.id)
  },
)

watch(triggerPointerInside, (inside) => {
  if (inside || !props.strictTriggerHover || !tooltipController.state.value.open) {
    return
  }
  tooltipController.close("pointer")
})

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

  <Teleport :to="teleportTarget || APP_OVERLAY_HOST_SELECTOR">
    <div
      v-if="!disabled && tooltipController.state.value.open"
      ref="tooltipRef"
      :class="tooltipClass"
      v-bind="tooltipProps"
      :style="tooltipStyle"
    >
      {{ text }}
    </div>
  </Teleport>
</template>
