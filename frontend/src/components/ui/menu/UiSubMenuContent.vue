<!-- File: UiSubMenuContent.vue -->
<script setup lang="ts">
import { ref, watch, nextTick } from "vue"
import { useStrictInject } from "./utils/useStrictInject"
import { UI_SUBMENU_KEY, type UiSubMenuContext } from "./submenuContext"

const ctx = useStrictInject<UiSubMenuContext>(UI_SUBMENU_KEY)

const root = ref<HTMLElement | null>(null)

/* ---------------- WATCH OPEN → POSITION + FOCUS ---------------- */

watch(
  () => ctx.open.value,
  async (v) => {
    if (v) {
      await nextTick()
      ctx.contentEl.value = root.value
      ctx.position()
      focusFirstItem()
    }
  }
)

function focusFirstItem() {
  const items = root.value?.querySelectorAll<HTMLElement>('[role="menuitem"]')
  if (items && items.length > 0) items[0].focus()
  else root.value?.focus()
}

/* ---------------- KEYBOARD NAVIGATION ---------------- */

function onKeydown(e: KeyboardEvent) {
  const items = Array.from(
    root.value?.querySelectorAll<HTMLElement>('[role="menuitem"]') ?? []
  )

  const active = document.activeElement as HTMLElement | null
  let i = items.indexOf(active!)

  if (e.key === "ArrowDown") {
    e.preventDefault()
    i = (i + 1 + items.length) % items.length
    items[i].focus()
  }

  if (e.key === "ArrowUp") {
    e.preventDefault()
    i = (i - 1 + items.length) % items.length
    items[i].focus()
  }

  if (e.key === "ArrowLeft") {
    ctx.closeMenu()
    ctx.parentItemEl.value?.focus()
  }
}

/* ---------------- SAFE CLOSE ON MOUSE LEAVE ---------------- */

function onPointerLeave(e: PointerEvent) {
  const target = e.relatedTarget as HTMLElement | null

  // Keep the submenu open when the pointer returns to the trigger so the user can re-enter without flicker.
  if (ctx.parentItemEl.value?.contains(target)) return

  ctx.scheduleClose()
}
</script>

<template>
  <teleport to="body">
    <div
      v-if="ctx.open.value"
      ref="root"
      class="ui-submenu-content"
      role="menu"
      tabindex="-1"
      @keydown="onKeydown"
      @pointerleave="onPointerLeave"
    >
      <slot />
    </div>
  </teleport>
</template>

<style>
.ui-submenu-content {
  position: absolute;
  min-width: 160px;
  background: var(--ui-menu-bg, #fff);
  border: 1px solid var(--ui-menu-border, #ddd);
  border-radius: 6px;
  box-shadow: 0 4px 12px rgba(0,0,0,0.15);
  outline: none;
  z-index: 999;
}
</style>
