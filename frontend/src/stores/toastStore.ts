import { defineStore } from "pinia"
import { ref } from "vue"

export type ToastVariant = "success" | "error" | "info"
export type ToastPosition =
  | "top-left"
  | "top-right"
  | "bottom-left"
  | "bottom-right"
  | "top-center"
  | "bottom-center"

export interface ToastItem {
  id: number
  message: string
  variant: ToastVariant
  timeout: number | null
  position: ToastPosition
}

const DEFAULT_TIMEOUT = 5000
const DEFAULT_POSITION: ToastPosition = "bottom-right"

export const useToastStore = defineStore("toastStore", () => {
  const toasts = ref<ToastItem[]>([])
  let seed = 0

  function push(
    message: string,
    options: Partial<Omit<ToastItem, "id" | "message">> = {}
  ): number {
    seed += 1
    const id = seed

    const toast: ToastItem = {
      id,
      message,
      variant: options.variant ?? "info",
      timeout: options.timeout ?? DEFAULT_TIMEOUT,
      position: options.position ?? DEFAULT_POSITION,
    }

    toasts.value.push(toast)

    if (toast.timeout !== null && typeof window !== "undefined") {
      window.setTimeout(() => remove(id), toast.timeout)
    }

    return id
  }

  function remove(id: number) {
    toasts.value = toasts.value.filter(t => t.id !== id)
  }

  function clear() {
    toasts.value = []
  }

  function getByPosition(position: ToastPosition) {
    return toasts.value.filter(t => t.position === position)
  }

  return {
    toasts,
    push,
    remove,
    clear,
    getByPosition,
  }
})
