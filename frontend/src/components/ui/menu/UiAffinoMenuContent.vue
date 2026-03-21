<template>
  <AffinoUiMenuContent
    :placement="placement"
    :align="align"
    :gutter="gutter"
    :viewport-padding="viewportPadding"
    :teleport-to="APP_OVERLAY_HOST_SELECTOR"
  >
    <slot />
  </AffinoUiMenuContent>
</template>

<script setup lang="ts">
import { computed } from "vue"
import {
  UiMenuContent as AffinoUiMenuContent,
  type Alignment,
  type Placement,
} from "@affino/menu-vue"
import { APP_OVERLAY_HOST_SELECTOR } from "@/utils/overlayHost"

const props = defineProps<{
  placement?: Placement
  align?: Alignment
  gutter?: number
  viewportPadding?: number
}>()

const placement = computed(() => props.placement)
const align = computed(() => props.align)
const gutter = computed(() => props.gutter)
const viewportPadding = computed(() => props.viewportPadding)
</script>
