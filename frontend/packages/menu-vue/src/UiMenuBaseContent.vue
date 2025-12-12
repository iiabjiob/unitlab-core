<script setup lang="ts">
import { Teleport, computed, nextTick, onBeforeUnmount, onMounted, ref, watch } from "vue"
import type { MenuProviderValue } from "./context"
import { useMenuPointerHandlers } from "./useMenuPointerHandlers"
import { useMenuFocus } from "./useMenuFocus"
import { useMenuPositioning } from "./useMenuPositioning"
import { toRect } from "./dom"
import { useSubmenuBridge } from "./useSubmenuBridge"

const props = defineProps<{
  provider: MenuProviderValue
  variant: "menu" | "submenu"
  teleportTo?: string
  className?: string
}>()

const root = ref<HTMLElement | null>(null)
const bindings = computed(() => props.provider.controller.core.getPanelProps())
const pointerHandlers = useMenuPointerHandlers(props.provider)
const submenuBridge = useSubmenuBridge(props.variant)
const panelPointer = pointerHandlers.makePanelHandlers({ bindings, bridge: submenuBridge })
const focus = useMenuFocus(props.provider.controller.panelRef)

const syncSubmenuGeometry = () => {
  if (props.variant !== "submenu" || !submenuBridge) {
    return
  }
  const triggerRect = toRect(submenuBridge.parent.controller.triggerRef.value)
  props.provider.controller.setTriggerRect?.(triggerRect ?? null)
  const panelRect = toRect(root.value)
  props.provider.controller.setPanelRect?.(panelRect ?? null)
}

const updatePosition = useMenuPositioning(props.provider.controller, syncSubmenuGeometry)

const teleportTarget = computed(() => props.teleportTo ?? "body")
const parentMenuId = props.provider.parentController?.id ?? ""

const refreshGeometry = () => {
  syncSubmenuGeometry()
  updatePosition()
}

watch(
  () => props.provider.controller.state.value.open,
  async (open) => {
    if (open) {
      await nextTick()
      props.provider.controller.panelRef.value = root.value
      refreshGeometry()
      focus.focusFirst()
    } else if (props.provider.controller.panelRef.value === root.value) {
      props.provider.controller.panelRef.value = null
      props.provider.controller.setAnchor(null)
    }
  }
)

watch(
  () => props.provider.controller.anchorRef.value,
  () => {
    if (props.provider.controller.state.value.open) {
      refreshGeometry()
    }
  }
)

onMounted(() => {
  if (props.provider.controller.state.value.open) {
    refreshGeometry()
  }
})

onBeforeUnmount(() => {
  if (props.provider.controller.panelRef.value === root.value) {
    props.provider.controller.panelRef.value = null
  }
})

function handlePointerEnter(event: PointerEvent) {
  panelPointer.onPointerEnter(event)
}

function handlePointerLeave(event: PointerEvent) {
  panelPointer.onPointerLeave(event)
}

function handleKeydown(event: KeyboardEvent) {
  if (event.key === "Tab") {
    event.preventDefault()
    if (props.variant === "submenu") {
      event.shiftKey ? focus.focusLast() : focus.focusFirst()
      return
    }
    props.provider.controller.close("keyboard")
    props.provider.controller.setAnchor(null)
    props.provider.controller.triggerRef.value?.focus()
    return
  }
  bindings.value.onKeyDown?.(event)
}
</script>

<template>
  <Teleport :to="teleportTarget">
    <div
      v-if="props.provider.controller.state.value.open"
      ref="root"
      :class="props.className ?? (props.variant === 'submenu' ? 'ui-submenu-content' : 'ui-menu-content')"
      :id="bindings.id"
      role="menu"
      tabindex="-1"
      data-ui-menu-panel="true"
      :data-ui-root-menu-id="props.provider.rootId"
      :data-ui-menu-id="props.provider.controller.id"
      :data-ui-parent-menu-id="parentMenuId"
      @pointerenter="handlePointerEnter"
      @pointerleave="handlePointerLeave"
      @keydown="handleKeydown"
    >
      <slot />
    </div>
  </Teleport>
</template>
