<script setup lang="ts">
import { ref, provide } from "vue"
import { UI_SUBMENU_KEY, type UiSubMenuContext } from "./submenuContext"

const open = ref(false)
const parentItemEl = ref<HTMLElement | null>(null)
const contentEl = ref<HTMLElement | null>(null)

let closeTimeout: number | null = null

function scheduleClose() {
  if (closeTimeout) clearTimeout(closeTimeout)
  closeTimeout = window.setTimeout(() => {
    open.value = false
  }, 150)
}

function cancelClose() {
  if (closeTimeout) clearTimeout(closeTimeout)
}

function openMenu() {
  cancelClose()
  open.value = true
}

function closeMenu() {
  cancelClose()
  open.value = false
}

function position() {
  if (!parentItemEl.value || !contentEl.value) return

  const t = parentItemEl.value.getBoundingClientRect()
  const c = contentEl.value.getBoundingClientRect()

  const left = t.right + 6
  let top = t.top

  const maxTop = window.innerHeight - c.height - 8
  top = Math.min(Math.max(8, top), maxTop)

  contentEl.value.style.left = `${left}px`
  contentEl.value.style.top = `${top}px`
}

const ctx: UiSubMenuContext = {
  open,
  parentItemEl,
  contentEl,
  openMenu,
  closeMenu,
  position,
  scheduleClose,
  cancelClose
}

provide(UI_SUBMENU_KEY, ctx)
</script>

<template>
  <slot />
</template>
