<script setup lang="ts">
import { computed, onBeforeUnmount, onMounted, ref, watch } from "vue"
import { useSubmenuContext } from "./context"
import { toRect } from "./dom"
import { toPointerPayload } from "./pointer"

const stopPropagationKeys = new Set(["ArrowDown", "ArrowUp", "ArrowLeft", "ArrowRight", "Home", "End", "Enter", " ", "Space"])

const submenuCtx = useSubmenuContext()
const el = ref<HTMLElement | null>(null)

const parentBindings = computed(() => {
  // depend on parent state so attributes refresh with highlight changes
  void submenuCtx.parent.state.value
  return submenuCtx.parent.core.getItemProps(submenuCtx.submenuItemId)
})

const triggerBindings = computed(() => submenuCtx.submenu.core.getTriggerProps())

const unregister = submenuCtx.parent.core.registerItem(submenuCtx.submenuItemId)

watch(
  () => submenuCtx.parent.state.value.activeItemId,
  (activeId) => {
    if (activeId === submenuCtx.submenuItemId) {
      el.value?.focus({ preventScroll: true })
    }
  },
  { flush: "sync" }
)

function handlePointerEnter(event: PointerEvent) {
  parentBindings.value.onPointerEnter?.(event)
  submenuCtx.submenu.core.recordPointer({ x: event.clientX, y: event.clientY })
  triggerBindings.value.onPointerEnter?.(toPointerPayload(event))
}

function handlePointerLeave(event: PointerEvent) {
  submenuCtx.submenu.core.recordPointer({ x: event.clientX, y: event.clientY })
  triggerBindings.value.onPointerLeave?.(toPointerPayload(event))
}

function handleKeydown(event: KeyboardEvent) {
  if (event.key === "ArrowLeft" && !submenuCtx.submenu.state.value.open && submenuCtx.parentSubmenu) {
    event.preventDefault()
    event.stopPropagation()
    submenuCtx.parentSubmenu.submenu.core.close("keyboard")
    submenuCtx.parentSubmenu.parent.core.highlight(submenuCtx.parentSubmenu.submenuItemId)
    submenuCtx.parentSubmenu.triggerRef.value?.focus({ preventScroll: true })
    return
  }

  if (stopPropagationKeys.has(event.key)) {
    event.stopPropagation()
  }

  parentBindings.value.onKeyDown?.(event)
  triggerBindings.value.onKeyDown?.(event)
}

function handleClick(event: MouseEvent) {
  submenuCtx.submenu.core.recordPointer({ x: event.clientX, y: event.clientY })
  triggerBindings.value.onClick?.(event)
}

onMounted(() => {
  submenuCtx.triggerRef.value = el.value
  if (el.value) {
    const rect = toRect(el.value)
    if (rect) submenuCtx.submenu.core.setTriggerRect(rect)
  }
})

onBeforeUnmount(() => {
  if (submenuCtx.triggerRef.value === el.value) {
    submenuCtx.triggerRef.value = null
  }
  unregister()
})
</script>

<template>
  <div
    ref="el"
    class="ui-submenu-trigger"
    :id="parentBindings.id"
    :role="parentBindings.role"
    :tabindex="parentBindings.tabIndex"
    :data-state="parentBindings['data-state']"
    :aria-disabled="parentBindings['aria-disabled']"
    :aria-haspopup="triggerBindings['aria-haspopup']"
    :aria-expanded="triggerBindings['aria-expanded']"
    :aria-controls="triggerBindings['aria-controls']"
    @pointerenter="handlePointerEnter"
    @pointerleave="handlePointerLeave"
    @keydown="handleKeydown"
    @click="handleClick"
  >
    <slot />
    <span class="ui-submenu-arrow">▶</span>
  </div>
</template>
