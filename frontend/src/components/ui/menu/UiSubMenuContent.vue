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