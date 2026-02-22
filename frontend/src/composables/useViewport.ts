import { ref, computed, onMounted, onBeforeUnmount } from "vue"

const DEFAULT_BREAKPOINTS = {
  tablet: 768,
  desktop: 1024,
}

type BreakpointConfig = typeof DEFAULT_BREAKPOINTS

const sharedWidth = ref(typeof window !== "undefined" ? window.innerWidth : DEFAULT_BREAKPOINTS.desktop)
const sharedHeight = ref(typeof window !== "undefined" ? window.innerHeight : 0)
let viewportConsumerCount = 0
let viewportResizeListenerBound = false

function syncSharedViewportSize() {
  if (typeof window === "undefined") return
  sharedWidth.value = window.innerWidth
  sharedHeight.value = window.innerHeight
}

function bindViewportResizeListener() {
  if (typeof window === "undefined" || viewportResizeListenerBound) return
  viewportResizeListenerBound = true
  window.addEventListener("resize", syncSharedViewportSize)
}

function unbindViewportResizeListener() {
  if (typeof window === "undefined" || !viewportResizeListenerBound) return
  viewportResizeListenerBound = false
  window.removeEventListener("resize", syncSharedViewportSize)
}

export function useViewport(breakpoints: BreakpointConfig = DEFAULT_BREAKPOINTS) {
  onMounted(() => {
    viewportConsumerCount += 1
    syncSharedViewportSize()
    bindViewportResizeListener()
  })

  onBeforeUnmount(() => {
    viewportConsumerCount = Math.max(0, viewportConsumerCount - 1)
    if (viewportConsumerCount === 0) {
      unbindViewportResizeListener()
    }
  })

  const isMobile = computed(() => sharedWidth.value < breakpoints.tablet)
  const isTablet = computed(() => sharedWidth.value >= breakpoints.tablet && sharedWidth.value < breakpoints.desktop)
  const isDesktop = computed(() => sharedWidth.value >= breakpoints.desktop)

  return {
    width: sharedWidth,
    height: sharedHeight,
    isMobile,
    isTablet,
    isDesktop,
  }
}
