import { nextTick, onBeforeUnmount, onMounted, watch } from "vue"
import type { MenuController } from "./useMenuController"
import { toRect, assignPanelPosition } from "./dom"

const isBrowser = typeof window !== "undefined"

export function useMenuPositioning(controller: MenuController, afterUpdate?: () => void) {
  if (!isBrowser) {
    return () => {}
  }

  let rafHandle: number | null = null
  let resizeObserver: ResizeObserver | null = null

  const schedule = () => {
    if (rafHandle !== null) return
    rafHandle = window.requestAnimationFrame(() => {
      rafHandle = null
      update()
    })
  }

  const update = () => {
    if (!controller.state.value.open) return
    const panelEl = controller.panelRef.value
    if (!panelEl) return
    const anchorRect = controller.anchorRef.value ?? toRect(controller.triggerRef.value)
    const panelRect = toRect(panelEl)
    if (!anchorRect || !panelRect) return
    const position = controller.core.computePosition(anchorRect, panelRect, {
      viewportWidth: window.innerWidth,
      viewportHeight: window.innerHeight,
    })
    assignPanelPosition(panelEl, position)
    afterUpdate?.()
  }

  const observe = (el: HTMLElement | null) => {
    if (!resizeObserver || !el) return
    resizeObserver.observe(el)
  }

  const unobserve = (el: HTMLElement | null) => {
    if (!resizeObserver || !el) return
    resizeObserver.unobserve(el)
  }

  if (typeof ResizeObserver !== "undefined") {
    resizeObserver = new ResizeObserver(schedule)

    watch(
      () => controller.triggerRef.value,
      (next, prev) => {
        if (prev) unobserve(prev)
        if (next) observe(next)
      }
    )

    watch(
      () => controller.panelRef.value,
      (next, prev) => {
        if (prev) unobserve(prev)
        if (next) observe(next)
      }
    )
  }

  const handleScroll = () => schedule()

  onMounted(() => {
    window.addEventListener("scroll", handleScroll, true)
    window.addEventListener("resize", handleScroll)
  })

  onBeforeUnmount(() => {
    if (rafHandle !== null) {
      window.cancelAnimationFrame(rafHandle)
      rafHandle = null
    }
    window.removeEventListener("scroll", handleScroll, true)
    window.removeEventListener("resize", handleScroll)
    resizeObserver?.disconnect()
  })

  watch(
    () => controller.state.value.open,
    (open) => {
      if (open) {
        nextTick(() => {
          schedule()
        })
      }
    }
  )

  watch(
    () => controller.anchorRef.value,
    () => {
      if (controller.state.value.open) {
        schedule()
      }
    }
  )

  return update
}
