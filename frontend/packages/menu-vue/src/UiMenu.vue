<script setup lang="ts">
import { ref, shallowRef } from "vue"
import type { MenuCallbacks, MenuOptions, Rect } from "@workspace/menu-core"
import { useMenu } from "./useMenu"
import { provideMenuContext } from "./context"

const props = defineProps<{ options?: MenuOptions; callbacks?: MenuCallbacks }>()

const triggerRef = ref<HTMLElement | null>(null)
const panelRef = ref<HTMLElement | null>(null)
const anchorOverride = shallowRef<Rect | null>(null)

const { core, state } = useMenu(props.options, props.callbacks)

provideMenuContext({
  core,
  state,
  triggerRef,
  panelRef,
  anchorOverride,
  parentMenuId: null,
  rootMenuId: core.id,
})
</script>

<template>
  <div class="ui-menu">
    <slot />
  </div>
</template>
