import type { TooltipController } from "@affino/tooltip-vue"

const tooltipControllers = new Map<string, TooltipController>()
let activeTooltipId: string | null = null
let globalDismissListenersAttached = false

export function registerTooltipController(controller: TooltipController): void {
  tooltipControllers.set(controller.id, controller)
  ensureGlobalDismissListeners()
}

export function unregisterTooltipController(controllerId: string): void {
  if (activeTooltipId === controllerId) {
    activeTooltipId = null
  }
  const controller = tooltipControllers.get(controllerId)
  if (controller?.state.value.open) {
    controller.close("programmatic")
  }
  tooltipControllers.delete(controllerId)
  if (tooltipControllers.size === 0) {
    removeGlobalDismissListeners()
  }
}

export function activateTooltipController(controllerId: string): void {
  activeTooltipId = controllerId
  tooltipControllers.forEach((controller, id) => {
    if (id === controllerId) {
      return
    }
    if (!controller.state.value.open) {
      return
    }
    controller.close("programmatic")
  })
}

export function deactivateTooltipController(controllerId: string): void {
  if (activeTooltipId === controllerId) {
    activeTooltipId = null
  }
}

export function closeAllTooltipControllers(reason: "pointer" | "keyboard" | "programmatic" = "programmatic"): void {
  activeTooltipId = null
  tooltipControllers.forEach((controller) => {
    if (!controller.state.value.open) {
      return
    }
    controller.close(reason)
  })
}

function ensureGlobalDismissListeners(): void {
  if (globalDismissListenersAttached || typeof window === "undefined" || typeof document === "undefined") {
    return
  }
  globalDismissListenersAttached = true
  window.addEventListener("pointerdown", handleGlobalPointerDown, true)
  window.addEventListener("scroll", handleGlobalScroll, true)
  window.addEventListener("resize", handleGlobalResize, true)
  window.addEventListener("blur", handleGlobalBlur, true)
  document.addEventListener("keydown", handleGlobalKeydown, true)
  document.addEventListener("visibilitychange", handleVisibilityChange, true)
}

function removeGlobalDismissListeners(): void {
  if (!globalDismissListenersAttached || typeof window === "undefined" || typeof document === "undefined") {
    return
  }
  globalDismissListenersAttached = false
  window.removeEventListener("pointerdown", handleGlobalPointerDown, true)
  window.removeEventListener("scroll", handleGlobalScroll, true)
  window.removeEventListener("resize", handleGlobalResize, true)
  window.removeEventListener("blur", handleGlobalBlur, true)
  document.removeEventListener("keydown", handleGlobalKeydown, true)
  document.removeEventListener("visibilitychange", handleVisibilityChange, true)
}

function handleGlobalPointerDown(): void {
  closeAllTooltipControllers("pointer")
}

function handleGlobalScroll(): void {
  closeAllTooltipControllers("programmatic")
}

function handleGlobalResize(): void {
  closeAllTooltipControllers("programmatic")
}

function handleGlobalBlur(): void {
  closeAllTooltipControllers("programmatic")
}

function handleGlobalKeydown(event: KeyboardEvent): void {
  if (event.key === "Escape") {
    closeAllTooltipControllers("keyboard")
  }
}

function handleVisibilityChange(): void {
  if (document.visibilityState !== "visible") {
    closeAllTooltipControllers("programmatic")
  }
}
