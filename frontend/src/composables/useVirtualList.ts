import {
  computed,
  nextTick,
  onBeforeUnmount,
  onMounted,
  ref,
  watch,
  type ComputedRef,
  type Ref,
} from "vue"

type MaybeReadonlyArray<T> = ReadonlyArray<T> | Array<T>
type SourceRef<T> = Ref<MaybeReadonlyArray<T>> | ComputedRef<MaybeReadonlyArray<T>>

type VirtualListOptions = {
  estimateSize: number
  overscan?: number
}

export type VirtualListItem<T> = {
  index: number
  item: T
}

export function useVirtualList<T>(
  source: SourceRef<T>,
  container: Ref<HTMLElement | null>,
  options: VirtualListOptions,
) {
  const estimateSize = Math.max(1, options.estimateSize)
  const overscan = Math.max(0, options.overscan ?? 8)

  const scrollTop = ref(0)
  const viewportHeight = ref(0)
  const sizeVersion = ref(0)
  const itemSizes = new Map<number, number>()
  const itemElements = new Map<number, HTMLElement>()
  const itemObservers = new Map<number, ResizeObserver>()
  let containerResizeObserver: ResizeObserver | null = null
  let cleanupScrollListener: (() => void) | null = null

  const itemCount = computed(() => source.value.length)

  const offsets = computed(() => {
    sizeVersion.value
    const nextOffsets = new Array<number>(itemCount.value + 1)
    nextOffsets[0] = 0
    for (let index = 0; index < itemCount.value; index += 1) {
      nextOffsets[index + 1] = nextOffsets[index] + (itemSizes.get(index) ?? estimateSize)
    }
    return nextOffsets
  })

  const totalSize = computed(() => offsets.value[itemCount.value] ?? 0)

  const visibleRange = computed(() => {
    const count = itemCount.value
    if (count === 0) {
      return { start: 0, end: 0 }
    }

    const startOffset = Math.max(0, scrollTop.value)
    const endOffset = Math.max(startOffset, startOffset + viewportHeight.value)
    const start = Math.max(0, findIndexAtOffset(offsets.value, startOffset) - overscan)
    const end = Math.min(count, findIndexAtOffset(offsets.value, endOffset) + overscan + 1)

    return { start, end: Math.max(start, end) }
  })

  const topSpacer = computed(() => offsets.value[visibleRange.value.start] ?? 0)
  const bottomSpacer = computed(() => {
    const endOffset = offsets.value[visibleRange.value.end] ?? totalSize.value
    return Math.max(0, totalSize.value - endOffset)
  })

  const virtualItems = computed<VirtualListItem<T>[]>(() => {
    const items = source.value
    const { start, end } = visibleRange.value
    const result: VirtualListItem<T>[] = []
    for (let index = start; index < end; index += 1) {
      const item = items[index]
      if (item !== undefined) {
        result.push({ index, item })
      }
    }
    return result
  })

  function updateViewport() {
    const el = container.value
    if (!el) {
      scrollTop.value = 0
      viewportHeight.value = 0
      return
    }
    scrollTop.value = el.scrollTop
    viewportHeight.value = el.clientHeight
  }

  function bindContainer(el: HTMLElement | null) {
    cleanupScrollListener?.()
    cleanupScrollListener = null
    containerResizeObserver?.disconnect()
    containerResizeObserver = null

    if (!el) {
      updateViewport()
      return
    }

    updateViewport()
    el.addEventListener("scroll", updateViewport, { passive: true })
    cleanupScrollListener = () => {
      el.removeEventListener("scroll", updateViewport)
    }

    if (typeof ResizeObserver !== "undefined") {
      containerResizeObserver = new ResizeObserver(updateViewport)
      containerResizeObserver.observe(el)
    }
  }

  function measureItem(index: number, el: HTMLElement) {
    const measuredSize = Math.ceil(el.getBoundingClientRect().height)
    if (!Number.isFinite(measuredSize) || measuredSize <= 0) {
      return
    }
    if (itemSizes.get(index) === measuredSize) {
      return
    }
    itemSizes.set(index, measuredSize)
    sizeVersion.value += 1
  }

  function clearItemElement(index: number) {
    const observer = itemObservers.get(index)
    if (observer) {
      observer.disconnect()
      itemObservers.delete(index)
    }
    itemElements.delete(index)
  }

  function setItemElement(index: number, el: HTMLElement | null) {
    const current = itemElements.get(index)
    if (current === el) {
      return
    }

    clearItemElement(index)

    if (!el) {
      return
    }

    itemElements.set(index, el)
    void nextTick().then(() => measureItem(index, el))

    if (typeof ResizeObserver !== "undefined") {
      const observer = new ResizeObserver(() => measureItem(index, el))
      observer.observe(el)
      itemObservers.set(index, observer)
    }
  }

  function resetMeasurements() {
    itemObservers.forEach(observer => observer.disconnect())
    itemObservers.clear()
    itemElements.clear()
    itemSizes.clear()
    sizeVersion.value += 1
  }

  watch(container, bindContainer, { immediate: false })
  watch(itemCount, (next, previous) => {
    if (next < previous) {
      resetMeasurements()
      return
    }
    for (const index of itemSizes.keys()) {
      if (index >= next) {
        itemSizes.delete(index)
        sizeVersion.value += 1
      }
    }
  })

  onMounted(() => {
    bindContainer(container.value)
  })

  onBeforeUnmount(() => {
    cleanupScrollListener?.()
    cleanupScrollListener = null
    containerResizeObserver?.disconnect()
    containerResizeObserver = null
    resetMeasurements()
  })

  return {
    bottomSpacer,
    setItemElement,
    topSpacer,
    totalSize,
    virtualItems,
  }
}

function findIndexAtOffset(offsets: readonly number[], offset: number) {
  let low = 0
  let high = Math.max(0, offsets.length - 2)

  while (low <= high) {
    const mid = Math.floor((low + high) / 2)
    const current = offsets[mid] ?? 0
    const next = offsets[mid + 1] ?? Number.POSITIVE_INFINITY

    if (current <= offset && offset < next) {
      return mid
    }
    if (current > offset) {
      high = mid - 1
    } else {
      low = mid + 1
    }
  }

  return Math.max(0, offsets.length - 2)
}
