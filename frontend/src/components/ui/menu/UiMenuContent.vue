<!-- File: UiMenuContent.vue -->
<script setup lang="ts">
import { inject, nextTick, onBeforeUnmount, ref, watch } from "vue"
import { UI_MENU_KEY, type UiMenuContext } from "./menuContext"

const injected = inject(UI_MENU_KEY)

if (!injected) {
  throw new Error("UiMenuContent must be used inside UiMenu")
}

const menu: UiMenuContext = injected
const root = ref<HTMLElement | null>(null)

// Keep the shared context up to date so the parent can reposition the floating panel when needed.
watch(
  () => root.value,
  (el) => {
    menu.contentEl.value = el
  }
)

onBeforeUnmount(() => {
  if (menu.contentEl.value === root.value) {
    menu.contentEl.value = null
  }
})

// As soon as the panel becomes visible, wait for the DOM to settle, align it, and focus the first item for a11y.
watch(
  () => menu.open.value,
  async (v) => {
    if (v) {
      await nextTick()
      menu.position()
      focusFirstItem()
    }
  }
)

// Keep focus inside the menu and provide a graceful fallback when no items are focusable yet.
function focusFirstItem() {
  const items = root.value?.querySelectorAll<HTMLElement>('[role="menuitem"]') ?? []
  if (items.length > 0) {
    items[0].focus()
  } else {
    root.value?.focus()
  }
}

// Keyboard navigation mirrors native menus: Escape closes, arrows cycle, Home/End jump.
function onKeydown(e: KeyboardEvent) {
  const items = root.value?.querySelectorAll<HTMLElement>('[role="menuitem"]') ?? []

  if (e.key === "Escape") {
    e.preventDefault()
    menu.close()
    menu.triggerEl.value?.focus()
    return
  }

  if (e.key === "Tab") {
    e.preventDefault()
    menu.close()
    menu.triggerEl.value?.focus()
    return
  }

  if (e.key === "ArrowDown") {
    e.preventDefault()
    move(1)
  }

  if (e.key === "ArrowUp") {
    e.preventDefault()
    move(-1)
  }

  if (e.key === "Home") {
    e.preventDefault()
    focusAt(0)
  }

  if (e.key === "End") {
    e.preventDefault()
    focusAt(items.length - 1)
  }

  if (e.key === "Enter" || e.key === " " || e.key === "Space") {
    const active = document.activeElement as HTMLElement | null
    active?.click()
  }

  function move(delta: number) {
    const arr = Array.from(items)
    if (arr.length === 0) return
    const active = document.activeElement as HTMLElement | null
    let i = active ? arr.indexOf(active) : -1
    if (i === -1) {
      i = delta > 0 ? 0 : arr.length - 1
    } else {
      // Wrap around so the navigation feels cyclical, just like native OS menus.
      i = (i + delta + arr.length) % arr.length
    }
    arr[i]?.focus()
  }

  function focusAt(index: number) {
    const arr = Array.from(items)
    if (arr.length === 0) return
    // Clamp to avoid exceptions when Home/End fire before the nodes exist or while filtering.
    const clamped = Math.max(0, Math.min(arr.length - 1, index))
    arr[clamped]?.focus()
  }
}
</script>

<template>
  <teleport to="body">
    <div
      v-if="menu.open.value"
      ref="root"
      class="ui-menu-content"
      :style="menu.menuStyle.value"
      role="menu"
      aria-orientation="vertical"
      tabindex="-1"
      @keydown="onKeydown"
    >
      <slot />
    </div>
  </teleport>
</template>
