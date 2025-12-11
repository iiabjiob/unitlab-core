<script setup lang="ts">
import { ref, onBeforeUnmount, shallowRef } from "vue"
import type { MenuCallbacks, MenuOptions, Rect } from "@workspace/menu-core"
import { createSubmenuController } from "./useMenu"
import { provideMenuContext, provideSubmenuContext, useMenuContext, useOptionalSubmenuContext } from "./context"
import { uid } from "./id"
import { usePointerRecorder } from "./usePointerRecorder"

const props = defineProps<{ id?: string; options?: MenuOptions; callbacks?: MenuCallbacks }>()

const parent = useMenuContext()
const submenuItemId = props.id ?? uid("ui-submenu-item")
const triggerRef = ref<HTMLElement | null>(null)
const panelRef = ref<HTMLElement | null>(null)
const anchorOverride = shallowRef<Rect | null>(null)

const resolvedOptions = props.options ?? {}

const { core, state, dispose } = createSubmenuController(
  parent.core,
  { ...resolvedOptions, parentItemId: submenuItemId },
  props.callbacks
)

usePointerRecorder(core)

const parentSubmenuCtx = useOptionalSubmenuContext()

const menuContextValue = {
  core,
  state,
  triggerRef,
  panelRef,
  anchorOverride,
  parentMenuId: parent.core.id,
  rootMenuId: parent.rootMenuId,
}

provideMenuContext(menuContextValue)
provideSubmenuContext({
  submenuItemId,
  submenu: { core, state },
  parent,
  triggerRef,
  panelRef,
  anchorOverride,
  parentSubmenu: parentSubmenuCtx,
})

onBeforeUnmount(() => {
  dispose()
})
</script>

<template>
  <slot />
</template>
