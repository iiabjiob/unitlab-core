<!-- File: UiMenu.vue -->
<script setup lang="ts">
import { nextTick, onBeforeUnmount, provide, ref, watch } from "vue"
import { UI_MENU_KEY, type UiMenuContext } from "./menuContext"

type Anchor = "trigger" | "cursor"

const open = ref(false)
const triggerEl = ref<HTMLElement | null>(null)
const contentEl = ref<HTMLElement | null>(null)
const menuStyle = ref<Record<string, string>>({})

const anchor = ref<Anchor>("trigger")
const cursorPoint = ref({ x: 0, y: 0 })

const emit = defineEmits<{
  (e: "open"): void
  (e: "close"): void
}>()

let listenersBound = false
let triggerResizeObserver: ResizeObserver | null = null
let contentResizeObserver: ResizeObserver | null = null

function openMenu() {
  if (!open.value) {
    open.value = true
    emit("open")
    bindGlobalListeners()
  }

  nextTick(() => {
    position()
  })
}

function closeMenu() {
  if (!open.value) return
  open.value = false
  emit("close")
  menuStyle.value = {}
  unbindGlobalListeners()
}

function openFromTrigger() {
  anchor.value = "trigger"
  openMenu()
}

function toggleFromTrigger() {
  if (open.value) {
    closeMenu()
    return
  }

  openFromTrigger()
}

function openAtCursor(e: MouseEvent) {
  e.preventDefault()
  cursorPoint.value = { x: e.clientX, y: e.clientY }
  anchor.value = "cursor"
  openMenu()
}

function position() {
  if (!contentEl.value) return

  const menuRect = contentEl.value.getBoundingClientRect()
  let left = 0
  let top = 0

  const padding = 8

  if (anchor.value === "cursor") {
    left = cursorPoint.value.x
    top = cursorPoint.value.y

    const maxLeft = window.innerWidth - menuRect.width - padding
    const maxTop = window.innerHeight - menuRect.height - padding

    left = Math.min(Math.max(padding, left), Math.max(padding, maxLeft))
    top = Math.min(Math.max(padding, top), Math.max(padding, maxTop))
  } else if (triggerEl.value) {
    const triggerRect = triggerEl.value.getBoundingClientRect()

    const preferredTop = triggerRect.bottom + 6
    const alternativeTop = triggerRect.top - menuRect.height - 6

    const willOverflowBottom = preferredTop + menuRect.height + padding > window.innerHeight
    const canOpenAbove = alternativeTop >= padding

    if (willOverflowBottom && canOpenAbove) {
      top = alternativeTop
    } else {
      top = preferredTop
    }

    left = triggerRect.left

    const maxLeft = window.innerWidth - menuRect.width - padding
    left = Math.min(Math.max(padding, left), Math.max(padding, maxLeft))

    const maxTop = window.innerHeight - menuRect.height - padding
    top = Math.min(Math.max(padding, top), Math.max(padding, maxTop))
  }

  menuStyle.value = {
    top: `${Math.round(top)}px`,
    left: `${Math.round(left)}px`
  }
}

function onPointerDown(e: PointerEvent) {
  if (!open.value) return
  const target = e.target as Node | null
  if (contentEl.value?.contains(target)) return
  if (triggerEl.value?.contains(target)) return
  closeMenu()
}

function onKeydown(e: KeyboardEvent) {
  if (e.key === "Escape") {
    e.preventDefault()
    closeMenu()
  }
}

function bindGlobalListeners() {
  if (listenersBound) return
  listenersBound = true

  document.addEventListener("pointerdown", onPointerDown, true)
  window.addEventListener("keydown", onKeydown)
  window.addEventListener("resize", position)
  window.addEventListener("scroll", position, true)
}

function unbindGlobalListeners() {
  if (!listenersBound) return
  listenersBound = false

  document.removeEventListener("pointerdown", onPointerDown, true)
  window.removeEventListener("keydown", onKeydown)
  window.removeEventListener("resize", position)
  window.removeEventListener("scroll", position, true)
}

watch(triggerEl, (el, prev) => {
  if (prev && triggerResizeObserver) {
    triggerResizeObserver.disconnect()
    triggerResizeObserver = null
  }

  if (el && typeof ResizeObserver !== "undefined") {
    triggerResizeObserver = new ResizeObserver(() => {
      if (open.value) position()
    })
    triggerResizeObserver.observe(el)
  }
})

watch(contentEl, (el, prev) => {
  if (prev && contentResizeObserver) {
    contentResizeObserver.disconnect()
    contentResizeObserver = null
  }

  if (el && typeof ResizeObserver !== "undefined") {
    contentResizeObserver = new ResizeObserver(() => {
      if (open.value) position()
    })
    contentResizeObserver.observe(el)
  }
})

const menuContext: UiMenuContext = {
  open,
  menuStyle,
  triggerEl,
  contentEl,
  position,
  openFromTrigger,
  openAtCursor,
  toggleFromTrigger,
  close: closeMenu
}

provide(UI_MENU_KEY, menuContext)

onBeforeUnmount(() => {
  unbindGlobalListeners()
  if (triggerResizeObserver) {
    triggerResizeObserver.disconnect()
    triggerResizeObserver = null
  }
  if (contentResizeObserver) {
    contentResizeObserver.disconnect()
    contentResizeObserver = null
  }
})
</script>

<template>
  <div class="ui-menu">
    <slot />
  </div>
</template>

<style>
:root {
  --ui-menu-bg: #ffffff;
  --ui-menu-border: #dddddd;
  --ui-menu-hover-bg: #f3f3f3;
  --ui-menu-text: #1f1f1f;
  --ui-menu-muted: #6b6b6b;
  --ui-menu-radius: 8px;
  --ui-menu-shadow: 0 10px 30px rgba(15, 23, 42, 0.12);
  --ui-menu-padding-y: 0.35rem;
  --ui-menu-item-radius: 6px;
  --ui-menu-separator: #e5e5e5;
  --ui-menu-danger: #d32f2f;
  --ui-menu-focus-ring: 0 0 0 2px rgba(65, 105, 225, 0.45);
  --ui-menu-submenu-indicator: #8c8c8c;
}

.ui-menu {
  position: relative;
  display: contents;
  font-family: inherit;
}

.ui-menu-trigger {
  display: inline-flex;
  align-items: center;
  gap: 0.25rem;
  padding: 0.375rem 0.65rem;
  border-radius: var(--ui-menu-radius, 8px);
  cursor: pointer;
  user-select: none;
  color: inherit;
  background: transparent;
}

.ui-menu-trigger:focus-visible {
  outline: none;
  box-shadow: var(--ui-menu-focus-ring);
}

.ui-menu-content {
  position: absolute;
  min-width: 180px;
  padding: var(--ui-menu-padding-y) 0;
  background: var(--ui-menu-bg, #fff);
  border: 1px solid var(--ui-menu-border, #ddd);
  border-radius: var(--ui-menu-radius, 8px);
  box-shadow: var(--ui-menu-shadow, 0 5px 20px rgba(0,0,0,0.15));
  outline: none;
  z-index: 999;
}

.ui-menu-item {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 0.65rem;
  padding: 0.45rem 0.9rem;
  font-size: 0.92rem;
  color: var(--ui-menu-text, #1f1f1f);
  cursor: pointer;
  border-radius: var(--ui-menu-item-radius, 6px);
  user-select: none;
}

.ui-menu-item:hover,
.ui-menu-item:focus-visible {
  background: var(--ui-menu-hover-bg, #f5f5f5);
  outline: none;
}

.ui-menu-item.is-danger {
  color: var(--ui-menu-danger, #c62828);
}

.ui-menu-label {
  padding: 0.35rem 0.9rem 0.2rem;
  font-size: 0.7rem;
  text-transform: uppercase;
  letter-spacing: 0.08em;
  color: var(--ui-menu-muted, #777);
  user-select: none;
}

.ui-menu-separator {
  height: 1px;
  margin: 0.35rem 0;
  background: var(--ui-menu-separator, #ececec);
}

.ui-submenu-trigger {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 0.65rem;
  padding: 0.45rem 0.9rem;
  border-radius: var(--ui-menu-item-radius, 6px);
  cursor: pointer;
  user-select: none;
  outline: none;
}

.ui-submenu-trigger:hover,
.ui-submenu-trigger:focus-visible {
  background: var(--ui-menu-hover-bg, #f5f5f5);
}

.ui-submenu-arrow {
  font-size: 0.75rem;
  color: var(--ui-menu-submenu-indicator, #8c8c8c);
}

.ui-submenu-content {
  position: absolute;
  min-width: 180px;
  padding: var(--ui-menu-padding-y) 0;
  background: var(--ui-menu-bg, #fff);
  border: 1px solid var(--ui-menu-border, #ddd);
  border-radius: var(--ui-menu-radius, 8px);
  box-shadow: var(--ui-menu-shadow, 0 5px 20px rgba(0,0,0,0.15));
  outline: none;
  z-index: 1000;
}
</style>
