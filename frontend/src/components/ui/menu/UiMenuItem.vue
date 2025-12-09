<script setup lang="ts">
import { inject } from "vue"
import { UI_MENU_KEY, type UiMenuContext } from "./menuContext"

const props = defineProps<{ danger?: boolean }>()

const emit = defineEmits(["select"])

const injected = inject(UI_MENU_KEY)

if (!injected) {
  throw new Error("UiMenuItem must be used inside UiMenu")
}

const menu: UiMenuContext = injected

function handleSelect() {
  emit("select")
  menu.close()
}

function onClick() {
  handleSelect()
}

function onKeydown(e: KeyboardEvent) {
  if (e.key === "Enter" || e.key === " " || e.key === "Space") {
    e.preventDefault()
    handleSelect()
  }
}

function onPointerEnter() {
  const active = document.activeElement as HTMLElement | null
  if (active && active !== document.body && active.getAttribute("role") === "menuitem") {
    active.blur()
  }
}
</script>

<template>
  <div
    class="px-3 py-2 text-sm cursor-pointer select-none rounded outline-none
           hover:bg-neutral-100 dark:hover:bg-neutral-700
           focus:bg-neutral-100 dark:focus:bg-neutral-700
           flex items-center gap-2"
    :class="props.danger ? 'text-red-600' : ''"
    role="menuitem"
    tabindex="-1"
    @click="onClick"
    @keydown="onKeydown"
    @pointerenter="onPointerEnter"
  >
    <slot />
  </div>
</template>
