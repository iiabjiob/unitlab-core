<script setup lang="ts">
import { nextTick, onBeforeUnmount, provide, ref } from "vue"
import { UI_MENU_KEY, type UiMenuContext } from "./menuContext"

type Anchor = "trigger" | "cursor"

const open = ref(false)
const triggerEl = ref<HTMLElement | null>(null)
const contentEl = ref<HTMLElement | null>(null)
const menuStyle = ref<Record<string, string>>({})

const anchor = ref<Anchor>("trigger")
const cursorPoint = ref({ x: 0, y: 0 })

const emit = defineEmits(["open", "close"])

function openMenu() {
  if (!open.value) {
    open.value = true
    emit("open")
    bindGlobalListeners()
  }

  nextTick(position)
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

  if (anchor.value === "cursor") {
    left = cursorPoint.value.x
    top = cursorPoint.value.y
  } else if (triggerEl.value) {
    const triggerRect = triggerEl.value.getBoundingClientRect()
    left = triggerRect.left
    top = triggerRect.bottom + 6
  }

  const padding = 8
  const maxLeft = window.innerWidth - menuRect.width - padding
  const maxTop = window.innerHeight - menuRect.height - padding

  left = Math.min(Math.max(padding, left), Math.max(padding, maxLeft))
  top = Math.min(Math.max(padding, top), Math.max(padding, maxTop))

  menuStyle.value = {
    top: `${Math.round(top)}px`,
    left: `${Math.round(left)}px`
  }
}

function onPointerDown(e: PointerEvent) {
  const target = e.target as Node | null
  if (!open.value) return
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
  document.addEventListener("pointerdown", onPointerDown, true)
  window.addEventListener("keydown", onKeydown)
  window.addEventListener("resize", position)
  window.addEventListener("scroll", position, true)
}

function unbindGlobalListeners() {
  document.removeEventListener("pointerdown", onPointerDown, true)
  window.removeEventListener("keydown", onKeydown)
  window.removeEventListener("resize", position)
  window.removeEventListener("scroll", position, true)
}

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
})
</script>

<template>
  <slot />
</template>
