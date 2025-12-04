import { onBeforeUnmount, ref, watch, type ComputedRef, type Ref } from "vue"
import { measureDomElements } from "../../core/utils/debug"

type MaybeHTMLElement = HTMLElement | null

interface UseDomSamplingOptions {
  containerRef: Ref<MaybeHTMLElement>
  active: ComputedRef<boolean>
  intervalMs?: number
}

interface UseDomSamplingResult {
  domElementCount: Ref<number>
  domLastUpdated: Ref<number>
  startDomSampling: () => void
  stopDomSampling: () => void
  sampleDomElements: (force?: boolean) => void
}

export function useDomSampling(options: UseDomSamplingOptions): UseDomSamplingResult {
  const domElementCount = ref(0)
  const domLastUpdated = ref(0)
  let domSampleHandle: number | null = null

  const sampleDomElements = (force = false) => {
    const container = options.containerRef.value
    if (!container) {
      if (force || domElementCount.value !== 0 || domLastUpdated.value !== 0) {
        domElementCount.value = 0
        domLastUpdated.value = 0
      }
      return
    }
    const nextCount = measureDomElements(container)
    if (force || nextCount !== domElementCount.value) {
      domElementCount.value = nextCount
      domLastUpdated.value = Date.now()
    }
  }

  const stopDomSampling = () => {
    if (typeof window !== "undefined" && domSampleHandle != null) {
      window.clearInterval(domSampleHandle)
    }
    domSampleHandle = null
  }

  const startDomSampling = () => {
    if (typeof window === "undefined") {
      sampleDomElements(true)
      return
    }
    if (domSampleHandle != null) {
      return
    }
    sampleDomElements(true)
    const interval = options.intervalMs ?? 1000
    domSampleHandle = window.setInterval(() => {
      sampleDomElements()
    }, interval)
  }

  watch(
    () => options.active.value,
    active => {
      if (active) {
        startDomSampling()
      } else {
        stopDomSampling()
      }
    },
    { immediate: true },
  )

  watch(
    options.containerRef,
    () => {
      if (!options.active.value) {
        return
      }
      sampleDomElements(true)
    },
  )

  onBeforeUnmount(() => {
    stopDomSampling()
  })

  return {
    domElementCount,
    domLastUpdated,
    startDomSampling,
    stopDomSampling,
    sampleDomElements,
  }
}
