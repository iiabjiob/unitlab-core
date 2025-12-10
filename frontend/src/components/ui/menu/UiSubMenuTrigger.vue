<script setup lang="ts">
import { ref, onMounted } from "vue"
import { useStrictInject } from "./utils/useStrictInject"
import { UI_SUBMENU_KEY, type UiSubMenuContext } from "./submenuContext"

const ctx = useStrictInject<UiSubMenuContext>(UI_SUBMENU_KEY)

const el = ref<HTMLElement | null>(null)

onMounted(() => {
  ctx.parentItemEl.value = el.value
})

/* ---------------- MOUSE TRAJECTORY (Amazon-style) ---------------- */

const mouse = [{ x: 0, y: 0 }, { x: 0, y: 0 }]

window.addEventListener("mousemove", (e) => {
  mouse.push({ x: e.clientX, y: e.clientY })
  if (mouse.length > 3) mouse.shift()
})

function movingTowardSubmenu(): boolean {
  if (!ctx.contentEl.value) return false

  const rect = ctx.contentEl.value.getBoundingClientRect()
  const p1 = mouse[0]
  const p2 = mouse[mouse.length - 1]

  const dx = p2.x - p1.x
  if (dx < 0) return false // движение не вправо

  const midY = (rect.top + rect.bottom) / 2
  return Math.abs(p2.y - midY) < rect.height / 2 + 40
}

/* ---------------- POINTER HANDLERS ---------------- */

function onPointerEnter() {
  ctx.cancelClose()
  ctx.openMenu()
}

function onPointerLeave(e: PointerEvent) {
  const target = e.relatedTarget as HTMLElement | null

  // Если уходим внутрь submenu → не закрывать
  if (ctx.contentEl.value?.contains(target)) return

  // Если движемся в сторону submenu → не закрывать
  if (movingTowardSubmenu()) return

  ctx.scheduleClose()
}

/* ---------------- KEYBOARD ---------------- */

function onKeydown(e: KeyboardEvent) {
  if (e.key === "ArrowRight" || e.key === "Enter" || e.key === " ") {
    e.preventDefault()
    ctx.openMenu()
  }

  if (e.key === "ArrowLeft") {
    ctx.closeMenu()
    ctx.parentItemEl.value?.focus()
  }
}
</script>

<template>
  <div
    ref="el"
    class="ui-submenu-trigger"
    role="menuitem"
    tabindex="-1"
    @pointerenter="onPointerEnter"
    @pointerleave="onPointerLeave"
    @keydown="onKeydown"
  >
    <slot />
    <span class="ui-submenu-arrow">▶</span>
  </div>
</template>

<style>
.ui-submenu-trigger {
  padding: 6px 12px;
  cursor: pointer;
  user-select: none;
  display: flex;
  justify-content: space-between;
  border-radius: 4px;
  outline: none;
}

.ui-submenu-trigger:hover,
.ui-submenu-trigger:focus {
  background: var(--ui-menu-hover-bg, #eee);
}

.ui-submenu-arrow {
  font-size: 12px;
  opacity: 0.6;
}
</style>
