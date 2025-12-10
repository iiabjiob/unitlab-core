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

// The trigger may either render its own wrapper or forward props to a child (`asChild`, à la Radix UI).
const props = defineProps<{ asChild?: boolean }>()

// Inject menu context
const injected = inject(UI_MENU_KEY)
if (!injected) {
  throw new Error("UiMenuTrigger must be used inside UiMenu")
}
const menu = injected as UiMenuContext

const slots = useSlots()
const triggerEl = ref<HTMLElement | null>(null)

/* -------------------------------------------------
 * UNIVERSAL REF HANDLER (fixes all DOM-related bugs)
 * ------------------------------------------------- */
function bind(el: Element | ComponentPublicInstance | null) {
  let element: HTMLElement | null = null

  // Case 1 — DOM element
  if (el instanceof HTMLElement) {
    element = el
  }

  // Case 2 — Vue component instance
  else if (el && (el as any).$el instanceof HTMLElement) {
    element = (el as any).$el
  }

  triggerEl.value = element
  menu.triggerEl.value = element
}

/* cleanup */
onBeforeUnmount(() => {
  if (menu.triggerEl.value === triggerEl.value) {
    menu.triggerEl.value = null
  }
})

/* handlers */
function onClick() {
  menu.toggleFromTrigger()
}

// Right-click opens the menu at the cursor — useful for context menus that share the same components.
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

/* --------------------------------------
 * asChild rendering (Radix-style cloning)
 * -------------------------------------- */
function renderAsChild() {
  const children = slots.default?.()

  if (!children || children.length !== 1) {
    throw new Error("UiMenuTrigger with asChild must have exactly one child")
  }

  const vnode = children[0]

  return cloneVNode(
    vnode,
    mergeProps(vnode.props || {}, {
      // Always ensure the semantic class is present so CSS variables keep working even in asChild mode.
      class: ["ui-menu-trigger", vnode.props?.class],
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
      // Correct and safe ref handler
      ref: (el: Element | ComponentPublicInstance | null) => {
        bind(el)
      }
    }),
    true
  )
}
</script>

<template>
  <!-- asChild: child becomes the trigger -->
  <template v-if="asChild">
    <component :is="renderAsChild()" />
  </template>

  <!-- default wrapper -->
  <div
    v-else
    :ref="bind"
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
