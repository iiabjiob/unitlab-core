<!-- File: UiSubMenu.vue -->
<script setup lang="ts">
import { ref, provide } from "vue"
import { UI_SUBMENU_KEY, type UiSubMenuContext } from "./submenuContext"

const id = `submenu-${Math.random().toString(36).slice(2)}`
const CLOSE_DELAY = 280
const OPEN_DELAY = 70

// Submenus manage their own open state so they can debounce pointer leave/enter without affecting the root menu.
const open = ref(false)
const parentItemEl = ref<HTMLElement | null>(null)
const contentEl = ref<HTMLElement | null>(null)

let closeTimeout: number | null = null
let openDelayTimeout: number | null = null

// Small delay mirrors native menus – allows users to move diagonally into the submenu without abrupt closes.
function scheduleClose() {
  if (closeTimeout) clearTimeout(closeTimeout)
  closeTimeout = window.setTimeout(() => {
    open.value = false
  }, CLOSE_DELAY)
}

function cancelClose() {
  if (closeTimeout) clearTimeout(closeTimeout)
}

function scheduleOpen() {
  if (openDelayTimeout) clearTimeout(openDelayTimeout)
  openDelayTimeout = window.setTimeout(() => {
    open.value = true
  }, OPEN_DELAY)
}

function cancelOpen() {
  if (openDelayTimeout) clearTimeout(openDelayTimeout)
}


function openMenu() {
  cancelClose()
  open.value = true
}

function closeMenu() {
  cancelClose()
  open.value = false
}

// Simple viewport-clamped positioning: submenu always opens to the right and adjusts vertically if needed.
function position() {
  if (!parentItemEl.value || !contentEl.value) return

  const trigger = parentItemEl.value.getBoundingClientRect()
  const panel = contentEl.value.getBoundingClientRect()

  // Default: open to the right
  let left = trigger.right + 6
  let top = trigger.top

  // If panel overflows right edge → open to the left
  const overflowRight = left + panel.width > window.innerWidth - 8
  if (overflowRight) {
    left = trigger.left - panel.width - 6
  }

  // Clamp vertically
  const maxTop = window.innerHeight - panel.height - 8
  top = Math.min(Math.max(8, top), maxTop)

  contentEl.value.style.left = `${left}px`
  contentEl.value.style.top = `${top}px`
}


const ctx: UiSubMenuContext = {
  id,
  open,
  parentItemEl,
  contentEl,
  openMenu,
  closeMenu,
  position,
  scheduleClose,
  scheduleOpen,
  cancelClose,
  cancelOpen,
}

provide(UI_SUBMENU_KEY, ctx)
</script>

<template>
  <slot />
</template>
