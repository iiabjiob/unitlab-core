import { inject, type InjectionKey } from "vue"

export function useStrictInject<T>(key: InjectionKey<T>): T {
  const v = inject(key)
  if (!v) {
    throw new Error(`Injection "${String(key)}" not found`)
  }
  return v
}
