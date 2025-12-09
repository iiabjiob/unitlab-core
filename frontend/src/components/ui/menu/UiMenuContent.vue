<script setup lang="ts">
import { inject, nextTick, onBeforeUnmount, ref, watch } from "vue"
import { UI_MENU_KEY, type UiMenuContext } from "./menuContext"

const injected = inject(UI_MENU_KEY)

if (!injected) {
  throw new Error("UiMenuContent must be used inside UiMenu")
}

const menu: UiMenuContext = injected
const root = ref<HTMLElement | null>(null)

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

watch(
  () => menu.open.value,
  async (v) => {
    if (v) {
      await nextTick()
      menu.position()
      root.value?.focus()
    }
  }
)

function onKeydown(e: KeyboardEvent) {
  const items = root.value?.querySelectorAll('[role="menuitem"]') ?? []

  if (e.key === "Escape") menu.close()

  if (e.key === "Tab") {
    e.preventDefault()
    menu.close()
    menu.triggerEl.value?.focus()
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
    (document.activeElement as HTMLElement)?.click()
  }

  function move(delta: number) {
    const arr = Array.from(items)
    if (arr.length === 0) return
    const active = document.activeElement as Element | null
    let i = active ? arr.indexOf(active) : -1
    if (i === -1) {
      i = delta > 0 ? 0 : arr.length - 1
    } else {
      i = (i + delta + arr.length) % arr.length
    }
    ;(arr[i] as HTMLElement).focus()
  }

  function focusAt(index: number) {
    const arr = Array.from(items)
    if (arr.length === 0) return
    const clamped = Math.max(0, Math.min(arr.length - 1, index))
    ;(arr[clamped] as HTMLElement | undefined)?.focus()
  }
}

function onPointerMove(e: PointerEvent) {
  const target = (e.target as HTMLElement | null)?.closest('[role="menuitem"]')
  if (!target) return
  const active = document.activeElement as HTMLElement | null
  if (active && active !== target && active.getAttribute("role") === "menuitem") {
    active.blur()
  }
}
</script>

<template>
  <teleport to="body">
    <div
      v-if="menu.open.value"
      ref="root"
      class="absolute z-50 min-w-[160px] rounded-md border border-neutral-200 bg-white shadow-lg 
             dark:border-neutral-700 dark:bg-neutral-800"
      :style="menu.menuStyle.value"
      role="menu"
      tabindex="-1"
      @keydown="onKeydown"
      @pointermove="onPointerMove"
    >
      <slot />
    </div>
  </teleport>
</template>
