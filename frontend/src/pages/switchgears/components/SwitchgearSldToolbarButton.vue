<script setup lang="ts">
import { computed, useAttrs } from "vue"

import UiHoverTooltip from "@/components/ui/UiHoverTooltip.vue"
import UiButton from "@/components/ui/UiButton.vue"

defineOptions({ inheritAttrs: false })

const attrs = useAttrs()
const tooltipText = computed(() => typeof attrs.title === "string" ? attrs.title : "")
const tooltipDisabled = computed(() => !tooltipText.value || attrs.disabled === true || attrs.disabled === "")
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
        v-bind="{ ...attrs, ...getTriggerProps() }"
      >
        <slot />
      </UiButton>
    </template>
  </UiHoverTooltip>
</template>
