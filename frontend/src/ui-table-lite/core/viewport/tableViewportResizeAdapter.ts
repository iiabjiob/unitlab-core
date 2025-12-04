import type {
  TableViewportHostEnvironment,
  TableViewportResizeObserver,
} from "./viewportHostEnvironment"

interface ManagedResizeObserver extends TableViewportResizeObserver {
  disconnect(): void
}

export function createManagedResizeObserver(
  hostEnvironment: TableViewportHostEnvironment,
  callback: () => void,
): TableViewportResizeObserver | null {
  const rawObserver = hostEnvironment.createResizeObserver?.(callback)
  if (!rawObserver) {
    return null
  }

  const observedTargets = new Set<Element>()

  const unobserveTarget = (target: Element) => {
    if (!observedTargets.has(target)) {
      return
    }
    hostEnvironment.removeResizeObserverTarget?.(rawObserver, target)
    rawObserver.unobserve?.(target)
    observedTargets.delete(target)
  }

  const adapter: ManagedResizeObserver = {
    observe(target: Element) {
      if (observedTargets.has(target)) {
        return
      }
      observedTargets.add(target)
      rawObserver.observe(target)
    },
    unobserve(target: Element) {
      unobserveTarget(target)
    },
    disconnect() {
      for (const target of observedTargets) {
        hostEnvironment.removeResizeObserverTarget?.(rawObserver, target)
        rawObserver.unobserve?.(target)
      }
      observedTargets.clear()
      rawObserver.disconnect()
    },
  }

  return adapter
}
