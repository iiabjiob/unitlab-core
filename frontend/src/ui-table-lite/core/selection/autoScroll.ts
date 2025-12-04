export interface PointerCoordinates {
  clientX: number
  clientY: number
}

export interface ViewportRect {
  left: number
  top: number
  right: number
  bottom: number
}

export interface AutoScrollAdapter {
  getViewportRect(): ViewportRect | null
  scrollBy(deltaX: number, deltaY: number): void
}

export interface AutoScrollOptions {
  threshold: number
  minSpeed: number
  maxSpeed: number
  adapter: AutoScrollAdapter
  scheduleFrame?: (callback: AutoScrollFrameCallback) => number
  cancelFrame?: (handle: number) => void
  onStep?: (state: { lastPointer: PointerCoordinates | null }) => void
}

export interface AutoScrollController {
  update(pointer: PointerCoordinates): void
  stop(): void
  getLastPointer(): PointerCoordinates | null
  isRunning(): boolean
}

const FALLBACK_FRAME_DURATION = 16

export type AutoScrollFrameCallback = (time: number) => void

function defaultScheduleFrame(callback: AutoScrollFrameCallback): number {
  if (typeof requestAnimationFrame === "function") {
    return requestAnimationFrame(callback)
  }
  return setTimeout(() => {
    const timestamp = typeof performance !== "undefined" ? performance.now() : Date.now()
    callback(timestamp)
  }, FALLBACK_FRAME_DURATION) as unknown as number
}

function defaultCancelFrame(handle: number) {
  if (typeof cancelAnimationFrame === "function") {
    cancelAnimationFrame(handle)
    return
  }
  clearTimeout(handle as unknown as ReturnType<typeof setTimeout>)
}

export function createAutoScrollController(options: AutoScrollOptions): AutoScrollController {
  const { threshold, minSpeed, maxSpeed, adapter } = options
  const scheduleFrame = options.scheduleFrame ?? defaultScheduleFrame
  const cancelFrame = options.cancelFrame ?? defaultCancelFrame

  let lastPointer: PointerCoordinates | null = null
  let velocityX = 0
  let velocityY = 0
  let frameHandle: number | null = null

  function computeSpeed(distance: number) {
    const clamped = Math.min(Math.max(distance, 0), threshold)
    if (clamped <= 0) return 0
    const ratio = clamped / threshold
    return minSpeed + (maxSpeed - minSpeed) * ratio
  }

  function scheduleIfNeeded() {
    if (frameHandle !== null) {
      if (velocityX === 0 && velocityY === 0) {
        cancelFrame(frameHandle)
        frameHandle = null
      }
      return
    }
    if (velocityX === 0 && velocityY === 0) {
      return
    }
    frameHandle = scheduleFrame(handleStep)
  }

  function handleStep(_time: number) {
    frameHandle = null
    if (velocityX === 0 && velocityY === 0) {
      return
    }

    adapter.scrollBy(velocityX, velocityY)
    options.onStep?.({ lastPointer })

    if (velocityX !== 0 || velocityY !== 0) {
      frameHandle = scheduleFrame(handleStep)
    }
  }

  function update(pointer: PointerCoordinates) {
    lastPointer = pointer
    const rect = adapter.getViewportRect()
    if (!rect) {
      velocityX = 0
      velocityY = 0
      scheduleIfNeeded()
      return
    }

    let nextVX = 0
    let nextVY = 0

    const leftThreshold = rect.left + threshold
    const rightThreshold = rect.right - threshold
    const topThreshold = rect.top + threshold
    const bottomThreshold = rect.bottom - threshold

    if (pointer.clientX < leftThreshold) {
      nextVX = -computeSpeed(leftThreshold - pointer.clientX)
    } else if (pointer.clientX > rightThreshold) {
      nextVX = computeSpeed(pointer.clientX - rightThreshold)
    }

    if (pointer.clientY < topThreshold) {
      nextVY = -computeSpeed(topThreshold - pointer.clientY)
    } else if (pointer.clientY > bottomThreshold) {
      nextVY = computeSpeed(pointer.clientY - bottomThreshold)
    }

    velocityX = nextVX
    velocityY = nextVY
    scheduleIfNeeded()
  }

  function stop() {
    velocityX = 0
    velocityY = 0
    if (frameHandle !== null) {
      cancelFrame(frameHandle)
      frameHandle = null
    }
    lastPointer = null
  }

  function getLastPointer() {
    return lastPointer
  }

  function isRunning() {
    return frameHandle !== null
  }

  return {
    update,
    stop,
    getLastPointer,
    isRunning,
  }
}
