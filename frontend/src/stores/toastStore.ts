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
  actionLabel: string | null
  onAction: (() => void) | null
}

const DEFAULT_TIMEOUT = 5000
const DEFAULT_POSITION: ToastPosition = "bottom-right"
type ToastOptions = Partial<Omit<ToastItem, "id" | "message">>
type ToastNoVariantOptions = Omit<ToastOptions, "variant">

export const useToastStore = defineStore("toastStore", () => {
  const toasts = ref<ToastItem[]>([])
  let seed = 0

  function push(message: string, options: ToastOptions = {}): number {
    seed += 1
    const id = seed

    const toast: ToastItem = {
      id,
      message,
      variant: options.variant ?? "info",
      timeout: options.timeout === undefined ? DEFAULT_TIMEOUT : options.timeout,
      position: options.position ?? DEFAULT_POSITION,
      actionLabel: typeof options.actionLabel === "string" && options.actionLabel.trim().length > 0
        ? options.actionLabel.trim()
        : null,
      onAction: typeof options.onAction === "function" ? options.onAction : null,
    }

    toasts.value.push(toast)

    if (toast.timeout !== null && typeof window !== "undefined") {
      window.setTimeout(() => remove(id), toast.timeout)
    }

    return id
  }

  function success(message: string, options: ToastNoVariantOptions = {}) {
    return push(message, { ...options, variant: "success" })
  }

  function error(message: string, options: ToastNoVariantOptions = {}) {
    return push(message, { ...options, variant: "error" })
  }

  function info(message: string, options: ToastNoVariantOptions = {}) {
    return push(message, { ...options, variant: "info" })
  }

  function warning(message: string, options: ToastNoVariantOptions = {}) {
    return push(message, { ...options, variant: "info" })
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
    success,
    error,
    info,
    warning,
    remove,
    clear,
    getByPosition,
  }
})
