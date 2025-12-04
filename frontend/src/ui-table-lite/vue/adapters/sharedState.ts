import { onScopeDispose, shallowRef, watch } from "vue"
import type { Ref } from "vue"

import type { SharedStateContainer } from "@/ui-table/core/state/sharedStateContainer"

export function useSharedStateProperty<State extends Record<string, any>, K extends keyof State>(
  container: SharedStateContainer<State>,
  key: K,
): Ref<State[K]> {
  const target = shallowRef<State[K]>(container.state[key])
  let pendingSyncs = 0

  const schedule = typeof queueMicrotask === "function"
    ? queueMicrotask
    : (callback: () => void) => {
        Promise.resolve().then(callback)
      }

  const unsubscribe = container.subscribeKey(key, value => {
    pendingSyncs += 1
    target.value = value
    schedule(() => {
      pendingSyncs = Math.max(0, pendingSyncs - 1)
    })
  })

  const stop = watch(
    target,
    value => {
      if (pendingSyncs > 0) return
      container.patch({ [key]: value } as Partial<State>)
    },
    { deep: false },
  )

  onScopeDispose(() => {
    unsubscribe()
    stop()
  })

  return target
}
