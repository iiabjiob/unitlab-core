import { ref, computed, onMounted, onBeforeUnmount } from "vue"

const DEFAULT_BREAKPOINTS = {
  tablet: 768,
  desktop: 1024,
}

type BreakpointConfig = typeof DEFAULT_BREAKPOINTS

export function useViewport(breakpoints: BreakpointConfig = DEFAULT_BREAKPOINTS) {
  const width = ref(typeof window !== "undefined" ? window.innerWidth : DEFAULT_BREAKPOINTS.desktop)
  const height = ref(typeof window !== "undefined" ? window.innerHeight : 0)

  function handleResize() {
    if (typeof window === "undefined") return
    width.value = window.innerWidth
    height.value = window.innerHeight
  }

  onMounted(() => {
    handleResize()
    window.addEventListener("resize", handleResize)
  })

  onBeforeUnmount(() => {
    window.removeEventListener("resize", handleResize)
  })

  const isMobile = computed(() => width.value < breakpoints.tablet)
  const isTablet = computed(() => width.value >= breakpoints.tablet && width.value < breakpoints.desktop)
  const isDesktop = computed(() => width.value >= breakpoints.desktop)

  return {
    width,
    height,
    isMobile,
    isTablet,
    isDesktop,
  }
}
