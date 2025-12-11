<!-- File: UiSubMenuTrigger.vue -->
<script setup lang="ts">
import { ref, onMounted, onUnmounted } from "vue"
import { useStrictInject } from "./utils/useStrictInject"
import { UI_SUBMENU_KEY, type UiSubMenuContext } from "./submenuContext"

const ctx = useStrictInject<UiSubMenuContext>(UI_SUBMENU_KEY)

const el = ref<HTMLElement | null>(null)

const MOUSE_HISTORY = 8
const VERTICAL_TOLERANCE = 48
const HEADING_THRESHOLD = 0.2

interface Point {
  x: number
  y: number
}

onMounted(() => {
  ctx.parentItemEl.value = el.value
})

/* ---------------- MOUSE PREDICTION ---------------- */
// We store last 5 mouse points to analyze direction
const mousePoints: Point[] = []
const handleMouseMove = (e: MouseEvent) => {
  mousePoints.push({ x: e.clientX, y: e.clientY })
  if (mousePoints.length > MOUSE_HISTORY) {
    mousePoints.shift()
  }
}

if (typeof window !== "undefined") {
  window.addEventListener("mousemove", handleMouseMove)
  onUnmounted(() => {
    window.removeEventListener("mousemove", handleMouseMove)
  })
}

// Main Amazon-style prediction
function isMovingTowardSubmenu(): boolean {
  if (!ctx.contentEl.value || !el.value) return false

  if (mousePoints.length < 2) return false

  const submenuRect = ctx.contentEl.value.getBoundingClientRect()
  const triggerRect = el.value.getBoundingClientRect()

  const last = mousePoints[mousePoints.length - 1]
  const comparisonIndex = Math.max(0, mousePoints.length - 4)
  const prev = mousePoints[comparisonIndex]
  const prevInstant = mousePoints[mousePoints.length - 2]

  const dx = last.x - prev.x
  const dy = last.y - prev.y
  const instantDx = last.x - prevInstant.x

  const opensRight = submenuRect.left >= triggerRect.right
  const directionSign = opensRight ? 1 : -1

  // Moving away from the submenu direction → allow close immediately
  if (instantDx * directionSign < -2) {
    return false
  }

  const movementMag = Math.hypot(dx, dy)
  if (movementMag === 0) return false

  const submenuCenterX = submenuRect.left + submenuRect.width / 2
  const submenuCenterY = submenuRect.top + submenuRect.height / 2
  const toCenterX = submenuCenterX - last.x
  const toCenterY = submenuCenterY - last.y
  const targetMag = Math.hypot(toCenterX, toCenterY)

  const heading =
    targetMag === 0
      ? true
      : (dx * toCenterX + dy * toCenterY) / (movementMag * targetMag) >
        HEADING_THRESHOLD

  const insideVertical =
    last.y >= submenuRect.top - VERTICAL_TOLERANCE &&
    last.y <= submenuRect.bottom + VERTICAL_TOLERANCE

  const horizontalProgress = opensRight
    ? last.x >= triggerRect.right - 6
    : last.x <= triggerRect.left + 6

  const driftBias = Math.abs(dx) > Math.abs(dy) * 0.4

  return insideVertical && (heading || horizontalProgress || driftBias)
}


/* ---------------- POINTER HANDLERS ---------------- */

function onPointerEnter() {
  ctx.cancelClose()
  ctx.scheduleOpen()
}

function onPointerLeave(e: PointerEvent) {
  const target = e.relatedTarget as HTMLElement | null

  if (ctx.contentEl.value?.contains(target)) return

  // MOST IMPORTANT FIX:
  if (isMovingTowardSubmenu()) return

  ctx.scheduleClose()
}

/* ---------------- KEYBOARD ---------------- */

function onKeydown(e: KeyboardEvent) {
  if (e.key === "ArrowRight" || e.key === "Enter" || e.key === " ") {
    e.preventDefault()
    ctx.openMenu()
  }

  if (e.key === "ArrowLeft") {
    e.preventDefault()
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
    :aria-haspopup="'menu'"
    :aria-controls="ctx.id"
    tabindex="-1"
    @pointerenter="onPointerEnter"
    @pointerleave="onPointerLeave"
    @keydown="onKeydown"
  >
    <slot />
    <span class="ui-submenu-arrow">▶</span>
  </div>
</template>
