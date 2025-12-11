<script setup lang="ts">
import { Teleport, computed, nextTick, onBeforeUnmount, onMounted, ref, watch } from "vue"
import { useMenuContext } from "./context"
import { assignPanelPosition, toRect } from "./dom"
import { toPointerPayload } from "./pointer"

const ctx = useMenuContext()
const root = ref<HTMLElement | null>(null)
const bindings = computed(() => ctx.core.getPanelProps())

function focusFirstItem() {
  const items = root.value?.querySelectorAll<HTMLElement>('[role="menuitem"]')
  if (!items || items.length === 0) {
    root.value?.focus()
    return
  }
  items[0].focus()
}

function getAnchorRect() {
  return ctx.anchorOverride.value ?? toRect(ctx.triggerRef.value)
}

function updatePosition() {
  if (!ctx.state.value.open) return
  if (!root.value) return
  const anchorRect = getAnchorRect()
  const panelRect = toRect(root.value)
  if (!anchorRect || !panelRect) return
  const position = ctx.core.computePosition(anchorRect, panelRect, {
    viewportWidth: window.innerWidth,
    viewportHeight: window.innerHeight,
  })
  assignPanelPosition(root.value, position)
}

function handlePointerEnter(event: PointerEvent) {
  bindings.value.onPointerEnter?.(toPointerPayload(event))
}

function isEnteringChildPanel(target: HTMLElement | null) {
  if (!target) return false
  const panel = target.closest<HTMLElement>("[data-ui-menu-panel='true']")
  if (!panel) return false
  return panel.getAttribute("data-ui-parent-menu-id") === ctx.core.id
}

function handlePointerLeave(event: PointerEvent) {
  if (!ctx.parentMenuId) {
    return
  }
  const related = event.relatedTarget as HTMLElement | null
  const meta = {
    isInsidePanel: related ? ctx.triggerRef.value?.contains(related) ?? false : false,
    enteredChildPanel: isEnteringChildPanel(related),
    relatedTargetId: related?.id ?? null,
  }
  bindings.value.onPointerLeave?.(toPointerPayload(event, meta))
}

function handleKeydown(event: KeyboardEvent) {
  if (event.key === "Tab") {
    event.preventDefault()
    ctx.core.close("keyboard")
    ctx.anchorOverride.value = null
    ctx.triggerRef.value?.focus()
    return
  }
  bindings.value.onKeyDown?.(event)
}

function isWithinMenuTree(target: EventTarget | null) {
  if (!(target instanceof Node)) return false
  if (ctx.triggerRef.value?.contains(target)) return true
  const element = target instanceof HTMLElement ? target : target.parentElement
  if (!element) return false
  const panel = element.closest<HTMLElement>("[data-ui-menu-panel='true']")
  if (!panel) return false
  return panel.getAttribute("data-ui-root-menu-id") === ctx.rootMenuId
}

function handleDocumentPointerDown(event: PointerEvent) {
  if (!ctx.state.value.open) return
  if (isWithinMenuTree(event.target)) return
  ctx.core.close("pointer")
}

watch(
  () => ctx.state.value.open,
  async (open) => {
    if (open) {
      await nextTick()
      ctx.panelRef.value = root.value
      updatePosition()
      focusFirstItem()
    } else if (ctx.panelRef.value === root.value) {
      ctx.panelRef.value = null
      ctx.anchorOverride.value = null
    }
  }
)

function reflow() {
  if (!ctx.state.value.open) return
  updatePosition()
}

onMounted(() => {
  window.addEventListener("resize", reflow)
  window.addEventListener("scroll", reflow, true)
  window.addEventListener("pointerdown", handleDocumentPointerDown, true)
})

onBeforeUnmount(() => {
  window.removeEventListener("resize", reflow)
  window.removeEventListener("scroll", reflow, true)
  window.removeEventListener("pointerdown", handleDocumentPointerDown, true)
  if (ctx.panelRef.value === root.value) {
    ctx.panelRef.value = null
  }
})
</script>

<template>
  <Teleport to="body">
    <div
      v-if="ctx.state.value.open"
      ref="root"
      class="ui-menu-content"
      :id="bindings.id"
      role="menu"
      tabindex="-1"
      data-ui-menu-panel="true"
      :data-ui-root-menu-id="ctx.rootMenuId"
      :data-ui-menu-id="ctx.core.id"
      :data-ui-parent-menu-id="ctx.parentMenuId ?? ''"
      @pointerenter="handlePointerEnter"
      @pointerleave="handlePointerLeave"
      @keydown="handleKeydown"
    >
      <slot />
    </div>
  </Teleport>
</template>
