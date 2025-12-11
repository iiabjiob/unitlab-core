<script setup lang="ts">
import { Teleport, computed, nextTick, onBeforeUnmount, onMounted, ref, watch } from "vue"
import { useMenuContext, useSubmenuContext } from "./context"
import { assignPanelPosition, toRect } from "./dom"
import { toPointerPayload } from "./pointer"

const ctx = useMenuContext()
const submenuCtx = useSubmenuContext()
const root = ref<HTMLElement | null>(null)
const bindings = computed(() => submenuCtx.submenu.core.getPanelProps())

function updateGeometry() {
  const triggerRect = toRect(submenuCtx.triggerRef.value)
  if (triggerRect) submenuCtx.submenu.core.setTriggerRect(triggerRect)
  const panelRect = toRect(root.value)
  if (panelRect) submenuCtx.submenu.core.setPanelRect(panelRect)
}

function updatePosition() {
  if (!submenuCtx.submenu.state.value.open) return
  const triggerRect = submenuCtx.triggerRef.value ? toRect(submenuCtx.triggerRef.value) : null
  const panelRect = root.value ? toRect(root.value) : null
  if (!triggerRect || !panelRect) return
  const position = submenuCtx.submenu.core.computePosition(triggerRect, panelRect, {
    viewportWidth: window.innerWidth,
    viewportHeight: window.innerHeight,
    preferSide: "right",
    gutter: 6,
  })
  assignPanelPosition(root.value, position)
  submenuCtx.submenu.core.setPanelRect({
    x: position.left,
    y: position.top,
    width: panelRect.width,
    height: panelRect.height,
  })
}

function focusFirstItem() {
  const items = root.value?.querySelectorAll<HTMLElement>('[role="menuitem"]')
  if (!items || items.length === 0) {
    root.value?.focus()
    return
  }
  items[0].focus()
}

function focusLastItem() {
  const items = root.value?.querySelectorAll<HTMLElement>('[role="menuitem"]')
  if (!items || items.length === 0) {
    root.value?.focus()
    return
  }
  items[items.length - 1].focus()
}

function handlePointerEnter(event: PointerEvent) {
  submenuCtx.submenu.core.recordPointer({ x: event.clientX, y: event.clientY })
  bindings.value.onPointerEnter?.(toPointerPayload(event))
}

function handlePointerLeave(event: PointerEvent) {
  const related = event.relatedTarget as HTMLElement | null
  const meta = {
    isInsidePanel: related ? submenuCtx.triggerRef.value?.contains(related) ?? false : false,
    enteredChildPanel: isEnteringChildPanel(related),
    relatedTargetId: related?.id ?? null,
  }
  submenuCtx.submenu.core.recordPointer({ x: event.clientX, y: event.clientY })
  bindings.value.onPointerLeave?.(toPointerPayload(event, meta))
}

function handleKeydown(event: KeyboardEvent) {
  if (event.key === "Tab") {
    event.preventDefault()
    if (event.shiftKey) {
      focusLastItem()
    } else {
      focusFirstItem()
    }
    return
  }
  bindings.value.onKeyDown?.(event)
}

function isEnteringChildPanel(target: HTMLElement | null) {
  if (!target) return false
  const panel = target.closest<HTMLElement>("[data-ui-menu-panel='true']")
  if (!panel) return false
  return panel.getAttribute("data-ui-parent-menu-id") === submenuCtx.submenu.core.id
}

watch(
  () => submenuCtx.submenu.state.value.open,
  async (open) => {
    if (open) {
      await nextTick()
      submenuCtx.panelRef.value = root.value
      updateGeometry()
      updatePosition()
      focusFirstItem()
    } else if (submenuCtx.panelRef.value === root.value) {
      submenuCtx.panelRef.value = null
    }
  }
)

function reflow() {
  if (!submenuCtx.submenu.state.value.open) return
  updatePosition()
}

onMounted(() => {
  window.addEventListener("resize", reflow)
  window.addEventListener("scroll", reflow, true)
})

onBeforeUnmount(() => {
  window.removeEventListener("resize", reflow)
  window.removeEventListener("scroll", reflow, true)
  if (submenuCtx.panelRef.value === root.value) {
    submenuCtx.panelRef.value = null
  }
})
</script>

<template>
  <Teleport to="body">
    <div
      v-if="submenuCtx.submenu.state.value.open"
      ref="root"
      class="ui-submenu-content"
      :id="bindings.id"
      role="menu"
      tabindex="-1"
      data-ui-menu-panel="true"
      :data-ui-root-menu-id="ctx.rootMenuId"
      :data-ui-menu-id="submenuCtx.submenu.core.id"
      :data-ui-parent-menu-id="ctx.core.id"
      @pointerenter="handlePointerEnter"
      @pointerleave="handlePointerLeave"
      @keydown="handleKeydown"
    >
      <slot />
    </div>
  </Teleport>
</template>
