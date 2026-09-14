<script setup lang="ts">
import { computed, useAttrs } from "vue"

import UiHoverTooltip from "@/components/ui/UiHoverTooltip.vue"
import UiButton from "@/components/ui/UiButton.vue"

defineOptions({ inheritAttrs: false })

const attrs = useAttrs()
const tooltipText = computed(() => typeof attrs.title === "string" ? attrs.title : "")
const tooltipDisabled = computed(() => !tooltipText.value || attrs.disabled === true || attrs.disabled === "")

function mergeEventHandlers(first: unknown, second: unknown) {
  const handlers = [first, second].flatMap((handler) => Array.isArray(handler) ? handler : [handler]).filter((handler): handler is (event: Event) => void => typeof handler === "function")
  return handlers.length === 0 ? undefined : (event: Event) => {
    for (const handler of handlers) {
      handler(event)
    }
  }
}

function getButtonAttrs(getTriggerProps: () => Record<string, unknown>) {
  const triggerProps = getTriggerProps()
  return {
    ...attrs,
    ...triggerProps,
    onClick: mergeEventHandlers(attrs.onClick, triggerProps.onClick),
    onPointerdown: mergeEventHandlers(attrs.onPointerdown, triggerProps.onPointerdown),
  }
}
</script>

<template>
  <UiHoverTooltip
    :text="tooltipText"
    :disabled="tooltipDisabled"
    placement="top"
    align="center"
    :suppress-open-on-click="true"
  >
    <template #default="{ setTriggerRef, getTriggerProps }">
      <UiButton
        :ref="setTriggerRef"
        v-bind="getButtonAttrs(getTriggerProps)"
      >
        <slot />
      </UiButton>
    </template>
  </UiHoverTooltip>
</template>
