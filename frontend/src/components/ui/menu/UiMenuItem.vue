<!-- File: UiMenuItem.vue -->
<script setup lang="ts">
import { inject, ref } from "vue"
import { UI_MENU_KEY, type UiMenuContext } from "./menuContext"

// `danger` is purely presentational (red text); actual destructive behavior lives in parent handlers.
const props = defineProps<{
  danger?: boolean
}>()

const emit = defineEmits<{
  (e: "select"): void
}>()

const injected = inject(UI_MENU_KEY)
if (!injected) {
  throw new Error("UiMenuItem must be used inside UiMenu")
}

const menu: UiMenuContext = injected

// Local ref for auto-scroll into view when navigated with keyboard.
const el = ref<HTMLElement | null>(null)

// Automatically scroll the item into view when it receives focus.
function onFocus() {
  // "nearest" ensures minimal scroll movement — ideal for menus.
  el.value?.scrollIntoView({ block: "nearest" })
}

// Emit `select` first so user handlers run before menu closes.
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
</script>

<template>
  <div
    ref="el"
    class="ui-menu-item"
    :class="{ 'is-danger': props.danger }"
    role="menuitem"
    tabindex="-1"
    @focus="onFocus"
    @click="onClick"
    @keydown="onKeydown"
  >
    <slot />
  </div>
</template>
