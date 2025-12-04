import { computed, onMounted, ref, unref } from "vue"
import type { CSSProperties, Ref } from "vue"
import { MAX_ZOOM, MIN_ZOOM, ZOOM_STEP, clamp } from "../../core/utils/constants"
import { supportsCssZoom } from "../../core/dom/gridUtils"

interface UseTableZoomOptions {
  tableId: string | Ref<string>
  emitZoomChange: (value: number) => void
  onZoomUpdated?: () => void
  focusContainer: () => void
}

// Manage viewport zoom level with persistence and wheel gestures
export function useTableZoom({ tableId, emitZoomChange, onZoomUpdated, focusContainer }: UseTableZoomOptions) {
  const zoom = ref(1)
  const storageKey = computed(() => {
    const id = unref(tableId)
    return id ? `uiTableZoom:${id}` : "uiTableZoom"
  })

  const zoomStyle = computed<CSSProperties>(() => {
    if (supportsCssZoom) {
      return {
        zoom: String(zoom.value),
      }
    }
    return {
      transform: `scale(${zoom.value})`,
      transformOrigin: "top left",
    }
  })

  // Keep zoom within allowed limits and normalize invalid entries
  function clampZoom(value: number) {
    return clamp(Number.isFinite(value) ? value : 1, MIN_ZOOM, MAX_ZOOM)
  }

  // Update zoom value, persist it, and notify listeners
  function setZoom(value: number) {
    zoom.value = clampZoom(value)
    if (typeof localStorage !== "undefined") {
      localStorage.setItem(storageKey.value, zoom.value.toString())
    }
    emitZoomChange(zoom.value)
    onZoomUpdated?.()
  }

  // Adjust zoom by a delta step (positive or negative)
  function adjustZoom(delta: number) {
    setZoom(zoom.value + delta)
  }

  // Handle ctrl/meta wheel gestures to zoom in or out
  function handleZoomWheel(event: WheelEvent) {
    if (!event.ctrlKey && !event.metaKey) return
    if (event.cancelable) {
      event.preventDefault()
    }
    const direction = event.deltaY < 0 ? 1 : -1
    adjustZoom(direction * ZOOM_STEP)
    focusContainer()
  }

  onMounted(() => {
    if (typeof localStorage === "undefined") return
    const storedZoomRaw = localStorage.getItem(storageKey.value)
    if (!storedZoomRaw) return
    const parsed = Number.parseFloat(storedZoomRaw)
    if (!Number.isFinite(parsed)) return
    zoom.value = clampZoom(parsed)
  })

  return {
    zoom: zoom as Ref<number>,
    zoomStyle,
    setZoom,
    adjustZoom,
    handleZoomWheel,
  }
}
