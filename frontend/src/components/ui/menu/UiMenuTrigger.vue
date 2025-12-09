<script setup lang="ts">
import { inject, onBeforeUnmount, onMounted, ref } from "vue"
import { UI_MENU_KEY, type UiMenuContext } from "./menuContext"

const injected = inject(UI_MENU_KEY)

if (!injected) {
  throw new Error("UiMenuTrigger must be used inside UiMenu")
}

const menu: UiMenuContext = injected
const trigger = ref<HTMLElement | null>(null)

onMounted(() => {
  menu.triggerEl.value = trigger.value!
})

onBeforeUnmount(() => {
  if (menu.triggerEl.value === trigger.value) {
    menu.triggerEl.value = null
  }
})

function onClick() {
  menu.toggleFromTrigger()
}

function onContextMenu(e: MouseEvent) {
  menu.openAtCursor(e)
}

function onKeydown(e: KeyboardEvent) {
  if (e.key === "Enter" || e.key === " " || e.key === "Space") {
    e.preventDefault()
    menu.toggleFromTrigger()
  }

  if (e.key === "ArrowDown") {
    e.preventDefault()
    menu.openFromTrigger()
  }
}
</script>

<template>
  <div
    ref="trigger"
    role="button"
    tabindex="0"
    aria-haspopup="menu"
    :aria-expanded="menu.open.value"
    @click="onClick"
    @contextmenu.prevent.stop="onContextMenu"
    @keydown="onKeydown"
  >
    <slot />
  </div>
</template>
