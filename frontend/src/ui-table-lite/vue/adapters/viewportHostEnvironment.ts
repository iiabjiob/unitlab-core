import type { TableViewportHostEnvironment } from "../../core/viewport/viewportHostEnvironment"

export function createViewportHostEnvironment(): TableViewportHostEnvironment {
  return {
    addScrollListener(target, listener, options) {
      if (typeof (target as any)?.addEventListener === "function") {
        ;(target as HTMLElement).addEventListener("scroll", listener, options ?? { passive: true })
      }
    },
    removeScrollListener(target, listener, options) {
      if (typeof (target as any)?.removeEventListener === "function") {
        const element = target as HTMLElement
        if (options !== undefined) {
          element.removeEventListener("scroll", listener, options)
        } else {
          element.removeEventListener("scroll", listener)
        }
      }
    },
    createResizeObserver(callback) {
      if (typeof ResizeObserver === "undefined") {
        return null
      }
      const observer = new ResizeObserver(() => callback())
      return {
        observe(target) {
          observer.observe(target)
        },
        unobserve(target) {
          observer.unobserve(target)
        },
        disconnect() {
          observer.disconnect()
        },
      }
    },
    removeResizeObserverTarget(observer, target) {
      if (typeof observer.unobserve === "function") {
        observer.unobserve(target)
      } else {
        observer.disconnect()
      }
    },
  }
}
