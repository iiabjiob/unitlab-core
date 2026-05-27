import { nextTick, onBeforeUnmount, onMounted, watch, type Ref, type ComputedRef } from "vue"

type MaybeReadonlyArray<T> = ReadonlyArray<T> | Array<T>

type SourceRef<T> = Ref<MaybeReadonlyArray<T>> | ComputedRef<MaybeReadonlyArray<T>>

type AutoScrollOptions = {
  bottomThresholdPx?: number
}

export function useAutoScroll<T>(
  source: SourceRef<T>,
  container: Ref<HTMLElement | null>,
  options: AutoScrollOptions = {},
) {
  const bottomThresholdPx = options.bottomThresholdPx ?? 32
  let autoScrollEnabled = true
  let cleanupScrollListener: (() => void) | null = null

  function isAtBottom(el: HTMLElement) {
    return el.scrollHeight - el.scrollTop - el.clientHeight <= bottomThresholdPx
  }

  function handleScroll() {
    const el = container.value
    if (!el) return
    autoScrollEnabled = isAtBottom(el)
  }

  function bindScrollListener(el: HTMLElement | null) {
    cleanupScrollListener?.()
    cleanupScrollListener = null

    if (!el) return
    autoScrollEnabled = isAtBottom(el)
    el.addEventListener("scroll", handleScroll, { passive: true })
    cleanupScrollListener = () => {
      el.removeEventListener("scroll", handleScroll)
    }
  }

  function scrollToBottom() {
    const el = container.value
    if (!el) return
    el.scrollTop = el.scrollHeight
    autoScrollEnabled = true
  }

  function scheduleScroll() {
    if (!autoScrollEnabled) return
    void nextTick().then(() => {
      if (autoScrollEnabled) {
        scrollToBottom()
      }
    })
  }

  onMounted(() => {
    bindScrollListener(container.value)
    scrollToBottom()
  })

  onBeforeUnmount(() => {
    cleanupScrollListener?.()
    cleanupScrollListener = null
  })

  watch(container, bindScrollListener, { immediate: false })
  watch(() => source.value.length, scheduleScroll, { immediate: false })
  watch(source, scheduleScroll, { immediate: false })
}
