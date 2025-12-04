type FrameHandle = number | ReturnType<typeof setTimeout>

type FrameCallback = () => void

export interface FrameRequestScheduler {
  request(callback: FrameCallback): number
  cancel(handle: number | null | undefined): void
  cancelAll(): void
  dispose(): void
}

export interface FrameRequestSchedulerOptions {
  globalObject?: typeof globalThis
  fallbackDelay?: number
}

const defaultGlobal: typeof globalThis = typeof window !== "undefined" ? window : globalThis

export function createFrameRequestScheduler(options: FrameRequestSchedulerOptions = {}): FrameRequestScheduler {
  const globalRef = options.globalObject ?? defaultGlobal
  const fallbackDelay = Number.isFinite(options.fallbackDelay) ? Number(options.fallbackDelay) : 16

  let nextHandle = 1
  const active = new Map<number, FrameHandle>()
  let disposed = false

  const requestAnimationFrameImpl =
    typeof globalRef.requestAnimationFrame === "function"
      ? (callback: FrameCallback) => globalRef.requestAnimationFrame(() => callback())
      : null

  const cancelAnimationFrameImpl =
    typeof globalRef.cancelAnimationFrame === "function" ? globalRef.cancelAnimationFrame.bind(globalRef) : null

  function request(callback: FrameCallback): number {
    if (disposed) return -1
    if (typeof callback !== "function") {
      throw new TypeError("Frame callback must be a function")
    }

    const handle = nextHandle
    nextHandle += 1

    if (requestAnimationFrameImpl) {
      const frameHandle = requestAnimationFrameImpl(() => {
        if (!active.delete(handle)) return
        callback()
      })
      active.set(handle, frameHandle)
      return handle
    }

    const timeoutHandle = globalRef.setTimeout(() => {
      if (!active.delete(handle)) return
      callback()
    }, fallbackDelay)
    active.set(handle, timeoutHandle)
    return handle
  }

  function cancel(handle: number | null | undefined): void {
    if (disposed || handle == null) return
    const frameHandle = active.get(handle)
    if (frameHandle === undefined) return

    active.delete(handle)
    if (typeof frameHandle === "number" && cancelAnimationFrameImpl) {
      cancelAnimationFrameImpl(frameHandle)
    } else {
      globalRef.clearTimeout(frameHandle as ReturnType<typeof setTimeout>)
    }
  }

  function cancelAll(): void {
    if (disposed) return
    for (const handle of Array.from(active.keys())) {
      cancel(handle)
    }
  }

  function dispose(): void {
    if (disposed) return
    cancelAll()
    disposed = true
  }

  return {
    request,
    cancel,
    cancelAll,
    dispose,
  }
}
