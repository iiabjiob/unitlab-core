import type { TooltipController } from "@affino/tooltip-vue"

const tooltipControllers = new Map<string, TooltipController>()
let activeTooltipId: string | null = null

export function registerTooltipController(controller: TooltipController): void {
  tooltipControllers.set(controller.id, controller)
}

export function unregisterTooltipController(controllerId: string): void {
  if (activeTooltipId === controllerId) {
    activeTooltipId = null
  }
  tooltipControllers.delete(controllerId)
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

