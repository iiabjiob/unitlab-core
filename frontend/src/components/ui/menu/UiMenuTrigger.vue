<!-- File: UiMenuTrigger.vue -->
<script setup lang="ts">
import {
  inject,
  ref,
  onBeforeUnmount,
  useSlots,
  cloneVNode,
  mergeProps,
  type ComponentPublicInstance
} from "vue"
import { UI_MENU_KEY, type UiMenuContext } from "./menuContext"

const props = defineProps<{ asChild?: boolean }>()

// Inject context
const injected = inject(UI_MENU_KEY)
if (!injected) {
  throw new Error("UiMenuTrigger must be used inside UiMenu")
}
const menu = injected as UiMenuContext

const slots = useSlots()
const triggerEl = ref<HTMLElement | null>(null)

function bind(el: HTMLElement | null) {
  triggerEl.value = el
  menu.triggerEl.value = el
}

onBeforeUnmount(() => {
  if (menu.triggerEl.value === triggerEl.value) {
    menu.triggerEl.value = null
  }
})

function onClick() {
  menu.toggleFromTrigger()
}

function onContextMenu(e: MouseEvent) {
  menu.openAtCursor(e)
}

function onKeydown(e: KeyboardEvent) {
  if (e.key === "Enter" || e.key === " " || e.key === "Space") {
    e.preventDefault()
    menu.toggleFromTrigger()
  }
  if (e.key === "ArrowDown") {
    e.preventDefault()
    menu.openFromTrigger()
  }
}

// --------------------------------------
// Generate cloned child VNode on demand
// --------------------------------------
function renderAsChild() {
  const children = slots.default?.()

  if (!children || children.length !== 1) {
    throw new Error("UiMenuTrigger with asChild must have exactly one child")
  }

  const vnode = children[0]

  return cloneVNode(
    vnode,
    mergeProps(vnode.props || {}, {
      class: "ui-menu-trigger",
      role: "button",
      tabindex: 0,
      "aria-haspopup": "menu",
      "aria-expanded": menu.open.value,
      onClick,
      onKeydown,
      onContextmenu: (e: MouseEvent) => {
        e.preventDefault()
        onContextMenu(e)
      },
      // IMPORTANT: correct ref signature
      ref: (el: Element | ComponentPublicInstance | null) => {
        bind(el as HTMLElement | null)
      }
    }),
    true
  )
}
</script>

<template>
  <!-- asChild mode: render cloned vnode inside render tree -->
  <template v-if="asChild">
    <component :is="renderAsChild()" />
  </template>

  <!-- default wrapper -->
  <div
    v-else
    ref="triggerEl"
    class="ui-menu-trigger"
    role="button"
    tabindex="0"
    aria-haspopup="menu"
    :aria-expanded="menu.open.value"
    @click="onClick"
    @keydown="onKeydown"
    @contextmenu.prevent.stop="onContextMenu"
  >
    <slot />
  </div>
</template>
