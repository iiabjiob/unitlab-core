<template>
  <AffinoUiMenu
    ref="affinoMenuRef"
    :options="mergedOptions"
    :callbacks="callbacks"
    :placement="placement"
    :align="align"
    :gutter="gutter"
    :viewport-padding="viewportPadding"
  >
    <slot />
  </AffinoUiMenu>
</template>

<script setup lang="ts">
import { computed, ref } from "vue"
import {
  UiMenu as AffinoUiMenu,
  type Alignment,
  type MenuCallbacks,
  type MenuController,
  type MenuOptions,
  type Placement,
} from "@affino/menu-vue"

const props = defineProps<{
  options?: MenuOptions
  callbacks?: MenuCallbacks
  placement?: Placement
  align?: Alignment
  gutter?: number
  viewportPadding?: number
}>()

const affinoMenuRef = ref<{ controller?: MenuController } | null>(null)

const defaultMenuOptions: MenuOptions = {
  mousePrediction: {},
  loopFocus: true,
  closeOnSelect: true,
}

const mergedOptions = computed(() => ({
  ...defaultMenuOptions,
  ...(props.options ?? {}),
}))

const callbacks = computed(() => props.callbacks)
const placement = computed(() => props.placement)
const align = computed(() => props.align)
const gutter = computed(() => props.gutter)
const viewportPadding = computed(() => props.viewportPadding)

defineExpose({
  get controller(): MenuController | undefined {
    return affinoMenuRef.value?.controller
  },
})
</script>
