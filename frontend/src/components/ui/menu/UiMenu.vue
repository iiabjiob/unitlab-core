<!-- File: UiMenu.vue -->
<script setup lang="ts">
import { nextTick, onBeforeUnmount, provide, ref, watch } from "vue"
import { UI_MENU_KEY, type UiMenuContext } from "./menuContext"

type Anchor = "trigger" | "cursor"

const open = ref(false)
const triggerEl = ref<HTMLElement | null>(null)
const contentEl = ref<HTMLElement | null>(null)
const menuStyle = ref<Record<string, string>>({})

// Track whether the menu should anchor to the trigger element or follow the cursor for context-menus.
const anchor = ref<Anchor>("trigger")
// Cursor coordinates are cached because the native event object becomes invalid after async work.
const cursorPoint = ref({ x: 0, y: 0 })

const emit = defineEmits<{
  (e: "open"): void
  (e: "close"): void
}>()

let listenersBound = false
let triggerResizeObserver: ResizeObserver | null = null
let contentResizeObserver: ResizeObserver | null = null

// Opening is async so we wait for the DOM to paint before reading layout for positioning.
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

// Compute a best-effort viewport-safe menu position for both trigger anchored and cursor anchored modes.
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

// Pointer capture is registered on the document so we only close when the click happens fully outside.
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

// The menu uses low level listeners rather than Vue events so nested portals still close correctly.
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

// Recompute the popover position when trigger metrics change (resizes, zoom, responsive layouts).
watch(triggerEl, (el, prev) => {
  if (prev && triggerResizeObserver) {
    triggerResizeObserver.disconnect()
    triggerResizeObserver = null
  }

  if (el instanceof HTMLElement && typeof ResizeObserver !== "undefined") {
    triggerResizeObserver = new ResizeObserver(() => {
      if (open.value) position()
    })
    triggerResizeObserver.observe(el)
  }
})


// Watch the floating panel as well because slot content can resize dynamically (icons, async labels, etc.).
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

// Provide the full reactive surface so nested components (trigger, content, items) can coordinate focus.
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
