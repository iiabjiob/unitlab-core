type MeasurementJob<T> = {
  measure: () => T
  resolve: (value: T) => void
  reject: (error: unknown) => void
  cancelled: () => boolean
}

export interface MeasurementHandle<T> {
  promise: Promise<T>
  cancel(): void
}

export interface MeasurementQueue {
  schedule<T>(measure: () => T): MeasurementHandle<T>
  flush(): void
  dispose(): void
}

export interface CreateMeasurementQueueOptions {
  globalObject?: typeof globalThis
}

export function createMeasurementQueue(
  options: CreateMeasurementQueueOptions = {},
): MeasurementQueue {
  const globalObject = options.globalObject ?? (typeof window !== "undefined" ? window : globalThis)
  const rafRef = (globalObject as typeof globalObject & { requestAnimationFrame?: typeof requestAnimationFrame }).requestAnimationFrame
  const cafRef = (globalObject as typeof globalObject & { cancelAnimationFrame?: typeof cancelAnimationFrame }).cancelAnimationFrame

  let rafHandle: number | null = null
  const jobs: MeasurementJob<unknown>[] = []
  let disposed = false

  function flushJobs() {
    if (!jobs.length) return
    const pending = jobs.splice(0, jobs.length)
    for (const job of pending) {
      if (job.cancelled()) continue
      try {
        const value = job.measure()
        job.resolve(value)
      } catch (error) {
        job.reject(error)
      }
    }
  }

  function ensureFrame() {
    if (disposed) return
    if (rafHandle !== null) return
    if (typeof rafRef !== "function") {
      flushJobs()
      return
    }
    rafHandle = rafRef.call(globalObject, () => {
      rafHandle = null
      flushJobs()
    })
  }

  function schedule<T>(measure: () => T): MeasurementHandle<T> {
    if (disposed || typeof rafRef !== "function") {
      const value = measure()
      return {
        promise: Promise.resolve(value),
        cancel() {
          // no-op once resolved synchronously
        },
      }
    }

    let cancelled = false
    const handle: MeasurementHandle<T> = {
      promise: new Promise<T>((resolve, reject) => {
        jobs.push({
          measure: measure as () => unknown,
          resolve: value => resolve(value as T),
          reject,
          cancelled: () => cancelled,
        })
        ensureFrame()
      }),
      cancel() {
        cancelled = true
      },
    }
    return handle
  }

  function flush() {
    if (rafHandle !== null && typeof cafRef === "function") {
      cafRef.call(globalObject, rafHandle)
    }
    rafHandle = null
    flushJobs()
  }

  function dispose() {
    if (disposed) return
    disposed = true
    if (rafHandle !== null && typeof cafRef === "function") {
      cafRef.call(globalObject, rafHandle)
    }
    rafHandle = null
    flushJobs()
  }

  return {
    schedule,
    flush,
    dispose,
  }
}

const defaultMeasurementQueue = createMeasurementQueue()

export function scheduleMeasurement<T>(measure: () => T): MeasurementHandle<T> {
  return defaultMeasurementQueue.schedule(measure)
}

export function flushMeasurements() {
  defaultMeasurementQueue.flush()
}

export function disposeDefaultMeasurementQueue() {
  defaultMeasurementQueue.dispose()
}
