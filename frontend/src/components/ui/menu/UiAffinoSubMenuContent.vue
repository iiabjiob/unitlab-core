<template>
  <AffinoUiSubMenuContent
    :placement="placement"
    :align="align"
    :gutter="gutter"
    :viewport-padding="viewportPadding"
  >
    <slot />
  </AffinoUiSubMenuContent>
</template>

<script setup lang="ts">
import { computed } from "vue"
import {
  UiSubMenuContent as AffinoUiSubMenuContent,
  type Alignment,
  type Placement,
} from "@affino/menu-vue"

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
