type IdleSchedulerHandle = number

type IdleSchedulerCallback = () => void

export interface IdleCallbackSchedulerOptions {
  globalObject?: typeof globalThis
  fallbackDelay?: number
  timeout?: number
}

export interface IdleCallbackScheduler {
  request(callback: IdleSchedulerCallback, options?: { timeout?: number }): number
  cancel(handle: number | null | undefined): void
  cancelAll(): void
  dispose(): void
}

const defaultGlobal: typeof globalThis = typeof window !== "undefined" ? window : globalThis

export function createIdleCallbackScheduler(options: IdleCallbackSchedulerOptions = {}): IdleCallbackScheduler {
  const globalRef = options.globalObject ?? defaultGlobal
  const fallbackDelay = Number.isFinite(options.fallbackDelay) ? Number(options.fallbackDelay) : 32
  const fallbackTimeout = Number.isFinite(options.timeout) ? Number(options.timeout) : 250

  let nextHandle = 1
  const active = new Map<number, IdleSchedulerHandle | ReturnType<typeof setTimeout>>()
  let disposed = false

  const requestIdleImpl = typeof globalRef.requestIdleCallback === "function" ? globalRef.requestIdleCallback.bind(globalRef) : null
  const cancelIdleImpl = typeof globalRef.cancelIdleCallback === "function" ? globalRef.cancelIdleCallback.bind(globalRef) : null

  function request(callback: IdleSchedulerCallback, requestOptions?: { timeout?: number }): number {
    if (disposed) return -1
    if (typeof callback !== "function") {
      throw new TypeError("Idle scheduler callback must be a function")
    }

    const handle = nextHandle
    nextHandle += 1

    if (requestIdleImpl) {
      const timeout = Number.isFinite(requestOptions?.timeout) ? Number(requestOptions?.timeout) : fallbackTimeout
      const idleHandle = requestIdleImpl(() => {
        if (!active.delete(handle)) return
        callback()
      }, { timeout })
      active.set(handle, idleHandle)
      return handle
    }

    const timeoutDelay = Number.isFinite(requestOptions?.timeout) ? Number(requestOptions?.timeout) : fallbackDelay
    const timeoutHandle = globalRef.setTimeout(() => {
      if (!active.delete(handle)) return
      callback()
    }, timeoutDelay)
    active.set(handle, timeoutHandle)
    return handle
  }

  function cancel(handle: number | null | undefined): void {
    if (disposed || handle == null) return
    const activeHandle = active.get(handle)
    if (activeHandle === undefined) return

    active.delete(handle)
    if (typeof activeHandle === "number" && cancelIdleImpl) {
      cancelIdleImpl(activeHandle)
    } else {
      globalRef.clearTimeout(activeHandle as ReturnType<typeof setTimeout>)
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
