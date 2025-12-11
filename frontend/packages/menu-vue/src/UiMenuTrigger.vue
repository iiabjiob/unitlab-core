<script setup lang="ts">
import { computed, onBeforeUnmount, ref, useSlots, cloneVNode } from "vue"
import type { ComponentPublicInstance } from "vue"
import { useMenuContext } from "./context"

const props = defineProps<{ asChild?: boolean; trigger?: "click" | "contextmenu" | "both" }>()

const ctx = useMenuContext()
const slots = useSlots()
const triggerEl = ref<HTMLElement | null>(null)
const bindings = computed(() => ctx.core.getTriggerProps())
const triggerMode = computed(() => props.trigger ?? "both")
const openOnClick = computed(() => triggerMode.value === "click" || triggerMode.value === "both")
const openOnContext = computed(() => triggerMode.value === "contextmenu" || triggerMode.value === "both")

function bind(el: Element | ComponentPublicInstance | null) {
  let element: HTMLElement | null = null
  if (el instanceof HTMLElement) {
    element = el
  } else if (el && (el as ComponentPublicInstance).$el instanceof HTMLElement) {
    element = (el as ComponentPublicInstance).$el as HTMLElement
  }
  triggerEl.value = element
  ctx.triggerRef.value = element
}

function resetAnchorOverride() {
  ctx.anchorOverride.value = null
}

function handleClick(event: MouseEvent) {
  if (!openOnClick.value) return
  resetAnchorOverride()
  bindings.value.onClick?.(event)
}

function handleKeydown(event: KeyboardEvent) {
  resetAnchorOverride()
  bindings.value.onKeyDown?.(event)
}

function setAnchorFromEvent(event: MouseEvent) {
  ctx.anchorOverride.value = {
    x: event.clientX,
    y: event.clientY,
    width: 0,
    height: 0,
  }
}

function handleContextMenu(event: MouseEvent) {
  if (!openOnContext.value) return
  event.preventDefault()
  if (ctx.state.value.open) {
    ctx.core.close("pointer")
    requestAnimationFrame(() => {
      setAnchorFromEvent(event)
      ctx.core.open("pointer")
    })
    return
  }
  setAnchorFromEvent(event)
  ctx.core.open("pointer")
}

function renderAsChild() {
  const children = slots.default?.()
  if (!children || children.length !== 1) {
    throw new Error("UiMenuTrigger with asChild must have exactly one child")
  }

  const vnode = children[0]
  return cloneVNode(vnode, createTriggerProps(false), true)
}

function createTriggerProps(includeBaseClass = true) {
  return {
    class: includeBaseClass ? "ui-menu-trigger" : undefined,
    id: bindings.value.id,
    role: bindings.value.role,
    tabindex: bindings.value.tabIndex,
    "aria-haspopup": bindings.value["aria-haspopup"],
    "aria-expanded": bindings.value["aria-expanded"],
    "aria-controls": bindings.value["aria-controls"],
    onClick: openOnClick.value ? handleClick : undefined,
    onKeydown: handleKeydown,
    onContextmenu: openOnContext.value ? handleContextMenu : undefined,
    ref: bind,
  }
}

onBeforeUnmount(() => {
  if (ctx.triggerRef.value === triggerEl.value) {
    ctx.triggerRef.value = null
  }
})
</script>

<template>
  <component v-if="props.asChild" :is="renderAsChild()" />
  <button
    v-else
    type="button"
    v-bind="createTriggerProps()"
  >
    <slot />
  </button>
</template>
