import { isRef, onBeforeUnmount, onMounted, ref, watch, watchEffect } from "vue"
import type { Ref } from "vue"
import type { TableOverlayScrollEmitter } from "./useTableOverlayScrollState"

export interface UseTableOverlayControllerOptions {
  scrollHostRef: Ref<HTMLElement | null>
  overlayState: TableOverlayScrollEmitter
  pinnedLeftOffset: Ref<number>
  pinnedRightOffset: Ref<number>
  enabled?: Ref<boolean> | boolean
}

export interface TableOverlayControllerHandle {
  requestSync(): void
}

const FLOAT_EPSILON = 1e-4

function valuesDiffer(a: number, b: number): boolean {
  return Math.abs(a - b) > FLOAT_EPSILON
}

export function useTableOverlayController(
  options: UseTableOverlayControllerOptions,
): TableOverlayControllerHandle {
  const enabledRef = isRef(options.enabled) ? options.enabled : ref(options.enabled ?? true)
  let frameId: number | null = null
  let framePending = false
  let disposed = false

  const scheduleFrame = () => {
    if (disposed) return
    if (!enabledRef.value) return
    if (framePending) return
    framePending = true
    frameId = requestAnimationFrame(flush)
  }

  const flush = () => {
    frameId = null
    framePending = false
    if (disposed || !enabledRef.value) {
      return
    }

    const host = options.scrollHostRef.value
    const width = host?.clientWidth ?? 0
    const height = host?.clientHeight ?? 0
    const scrollLeft = host?.scrollLeft ?? 0
    const scrollTop = host?.scrollTop ?? 0
    const pinnedLeft = options.pinnedLeftOffset.value
    const pinnedRight = options.pinnedRightOffset.value

    const snapshot = options.overlayState.snapshot.value
    if (
      valuesDiffer(snapshot.viewportWidth, width) ||
      valuesDiffer(snapshot.viewportHeight, height) ||
      valuesDiffer(snapshot.scrollLeft, scrollLeft) ||
      valuesDiffer(snapshot.scrollTop, scrollTop) ||
      valuesDiffer(snapshot.pinnedOffsetLeft, pinnedLeft) ||
      valuesDiffer(snapshot.pinnedOffsetRight, pinnedRight)
    ) {
      options.overlayState.emit({
        viewportWidth: width,
        viewportHeight: height,
        scrollLeft,
        scrollTop,
        pinnedOffsetLeft: pinnedLeft,
        pinnedOffsetRight: pinnedRight,
      })
    }
  }

  const requestSync = () => {
    scheduleFrame()
  }

  watch(
    () => options.pinnedLeftOffset.value,
    () => requestSync(),
    { flush: "sync" },
  )

  watch(
    () => options.pinnedRightOffset.value,
    () => requestSync(),
    { flush: "sync" },
  )

  watch(
    enabledRef,
    value => {
      if (!value) {
        if (framePending && frameId != null) {
          cancelAnimationFrame(frameId)
          frameId = null
          framePending = false
        }
        return
      }
      requestSync()
    },
    { flush: "sync" },
  )

  watchEffect(onCleanup => {
    const host = options.scrollHostRef.value
    if (!host) {
      return
    }

    const handleScroll = () => requestSync()
    host.addEventListener("scroll", handleScroll, { passive: true })

    let resizeObserver: ResizeObserver | null = null
    if (typeof ResizeObserver !== "undefined") {
      resizeObserver = new ResizeObserver(() => requestSync())
      resizeObserver.observe(host)
    }

    requestSync()

    onCleanup(() => {
      host.removeEventListener("scroll", handleScroll)
      resizeObserver?.disconnect()
    })
  })

  onMounted(() => {
    requestSync()
  })

  onBeforeUnmount(() => {
    disposed = true
    if (framePending && frameId != null) {
      cancelAnimationFrame(frameId)
    }
  })

  return {
    requestSync,
  }
}
