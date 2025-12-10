<!-- File: UiMenuItem.vue -->
<script setup lang="ts">
import { inject } from "vue"
import { UI_MENU_KEY, type UiMenuContext } from "./menuContext"

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
    class="ui-menu-item"
    :class="{ 'is-danger': props.danger }"
    role="menuitem"
    tabindex="-1"
    @click="onClick"
    @keydown="onKeydown"
  >
    <slot />
  </div>
</template>
