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
  if (items && items.length > 0) {
    items[0].focus()
    // AUTO-SCROLL on initial open
    items[0].scrollIntoView({ block: "nearest" })
  } else {
    root.value?.focus()
  }
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
    // AUTO-SCROLL
    items[i].scrollIntoView({ block: "nearest" })
  }

  if (e.key === "ArrowUp") {
    e.preventDefault()
    i = (i - 1 + items.length) % items.length
    items[i].focus()
    // AUTO-SCROLL
    items[i].scrollIntoView({ block: "nearest" })
  }

  if (e.key === "Home") {
    e.preventDefault()
    if (items.length > 0) {
      items[0].focus()
      // AUTO-SCROLL
      items[0].scrollIntoView({ block: "nearest" })
    }
  }

  if (e.key === "End") {
    e.preventDefault()
    if (items.length > 0) {
      items[items.length - 1].focus()
      // AUTO-SCROLL
      items[items.length - 1].scrollIntoView({ block: "nearest" })
    }
  }

  if (e.key === "Escape") {
    e.preventDefault()
    ctx.closeMenu()
    ctx.parentItemEl.value?.focus()
    return
  }

  if (e.key === "ArrowLeft") {
    ctx.closeMenu()
    ctx.parentItemEl.value?.focus()
  }

  // Focus trap (Tab → first/last item)
  if (e.key === "Tab") {
    e.preventDefault()
    if (!items.length) return
    e.shiftKey ? items[items.length - 1].focus() : items[0].focus()
  }
}

/* ---------------- SAFE CLOSE ON MOUSE LEAVE ---------------- */

function isMovingIntoChildSubmenu(target: HTMLElement | null) {
  if (!target || !root.value) return false

  const childPanel = target.closest<HTMLElement>(".ui-submenu-content")
  if (!childPanel || !childPanel.id) return false

  return Boolean(
    root.value.querySelector<HTMLElement>(`[aria-controls="${childPanel.id}"]`)
  )
}

function onPointerEnter() {
  ctx.cancelClose()
}

function onPointerLeave(e: PointerEvent) {
  const target = e.relatedTarget as HTMLElement | null

  if (!target) {
    ctx.scheduleClose()
    return
  }

  if (ctx.parentItemEl.value?.contains(target)) {
    ctx.cancelClose()
    return
  }

  if (isMovingIntoChildSubmenu(target)) {
    ctx.cancelClose()
    return
  }

  ctx.scheduleClose()
}
</script>

<template>
  <teleport to="body">
    <div
      v-if="ctx.open.value"
      ref="root"
      class="ui-submenu-content"
      :id="ctx.id"
      role="menu"
      tabindex="-1"
      @pointerenter="onPointerEnter"
      @keydown="onKeydown"
      @pointerleave="onPointerLeave"
    >
      <slot />
    </div>
  </teleport>
</template>