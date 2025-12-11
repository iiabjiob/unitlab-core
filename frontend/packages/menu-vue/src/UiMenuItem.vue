<script setup lang="ts">
import { computed, onBeforeUnmount, ref, watch } from "vue"
import { useMenuContext, useOptionalSubmenuContext } from "./context"
import { uid } from "./id"

const stopPropagationKeys = new Set(["ArrowDown", "ArrowUp", "Home", "End", "Enter", " ", "Space"])

const props = defineProps<{ id?: string; disabled?: boolean; danger?: boolean }>()
const emit = defineEmits<{ (e: "select"): void }>()

const ctx = useMenuContext()
const submenuCtx = useOptionalSubmenuContext()
const itemId = props.id ?? uid("ui-menu-item")
const unregister = ctx.core.registerItem(itemId, { disabled: props.disabled })

watch(
  () => props.disabled,
  (value) => {
    ctx.core.registerItem(itemId, { disabled: value })
  }
)

const bindings = computed(() => {
  // depend on reactive menu state so props refresh when highlight changes
  void ctx.state.value
  return ctx.core.getItemProps(itemId)
})
const el = ref<HTMLElement | null>(null)

function handleFocus() {
  el.value?.scrollIntoView({ block: "nearest" })
}

const isDisabled = computed(() => Boolean(bindings.value["aria-disabled"]))

watch(
  () => ctx.state.value.activeItemId,
  (activeId) => {
    if (activeId === itemId && !isDisabled.value) {
      el.value?.focus({ preventScroll: true })
    }
  },
  { flush: "sync" }
)

function handleClick(event: MouseEvent) {
  if (isDisabled.value) {
    event.preventDefault()
    return
  }
  emit("select")
  bindings.value.onClick?.(event)
}

function handlePointerEnter(event: PointerEvent) {
  bindings.value.onPointerEnter?.(event)
}

function handleKeydown(event: KeyboardEvent) {
  if (!isDisabled.value && (event.key === "Enter" || event.key === " " || event.key === "Space")) {
    emit("select")
  }
  if (submenuCtx && event.key === "ArrowLeft") {
    event.preventDefault()
    event.stopPropagation()
    submenuCtx.submenu.core.close("keyboard")
    submenuCtx.parent.core.highlight(submenuCtx.submenuItemId)
    submenuCtx.triggerRef.value?.focus({ preventScroll: true })
    return
  }
  if (stopPropagationKeys.has(event.key)) {
    event.stopPropagation()
  }
  bindings.value.onKeyDown?.(event)
}

onBeforeUnmount(() => {
  unregister()
})
</script>

<template>
  <div
    ref="el"
    class="ui-menu-item"
    :class="{ 'is-danger': props.danger }"
    :id="bindings.id"
    :role="bindings.role"
    :tabindex="bindings.tabIndex"
    :data-state="bindings['data-state']"
    :aria-disabled="bindings['aria-disabled']"
    @focus="handleFocus"
    @pointerenter="handlePointerEnter"
    @click="handleClick"
    @keydown="handleKeydown"
  >
    <slot />
  </div>
</template>
