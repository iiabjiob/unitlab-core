import { scheduleMeasurement } from "../../core/runtime/measurementQueue"
import type { HoverDelegationEnvironment } from "../../core/dom/domAdapters"

const defaultGlobalObject: typeof globalThis = typeof window !== "undefined" ? window : globalThis

export function createHoverDelegationEnvironment(target: typeof globalThis = defaultGlobalObject): HoverDelegationEnvironment {
  return {
    measurement: {
      measure: callback => scheduleMeasurement(callback).promise,
    },
    frame: {
      schedule: callback => {
        if (typeof target.requestAnimationFrame === "function") {
          return target.requestAnimationFrame(() => callback())
        }
        return target.setTimeout(callback, 16) as unknown as number
      },
      cancel: handle => {
        if (typeof target.cancelAnimationFrame === "function") {
          target.cancelAnimationFrame(handle)
          return
        }
        target.clearTimeout(handle as unknown as ReturnType<typeof setTimeout>)
      },
    },
  }
}
