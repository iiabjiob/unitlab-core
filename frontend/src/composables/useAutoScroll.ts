import { nextTick, onMounted, watch, type Ref, type ComputedRef } from "vue"

type MaybeReadonlyArray<T> = ReadonlyArray<T> | Array<T>

type SourceRef<T> = Ref<MaybeReadonlyArray<T>> | ComputedRef<MaybeReadonlyArray<T>>

export function useAutoScroll<T>(source: SourceRef<T>, container: Ref<HTMLElement | null>) {
  function scrollToBottom() {
    const el = container.value
    if (!el) return
    el.scrollTop = el.scrollHeight
  }

  function scheduleScroll() {
    void nextTick().then(scrollToBottom)
  }

  onMounted(scrollToBottom)

  watch(source, scheduleScroll, { immediate: false })
}
