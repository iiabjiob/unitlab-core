import type { ClipboardAdapter } from "../../core/dom/domAdapters"

export function createClipboardAdapter(): ClipboardAdapter {
  return {
    async writeText(value: string) {
      if (typeof navigator !== "undefined" && navigator.clipboard?.writeText) {
        await navigator.clipboard.writeText(value)
        return
      }
      if (typeof document === "undefined") {
        throw new Error("Clipboard API unavailable")
      }
      const textarea = document.createElement("textarea")
      textarea.value = value
      textarea.setAttribute("readonly", "")
      textarea.style.position = "fixed"
      textarea.style.opacity = "0"
      document.body.appendChild(textarea)
      textarea.select()
      try {
        const execCommand = (document as any).execCommand as ((command: string) => unknown) | undefined
        if (typeof execCommand === "function") {
          execCommand.call(document, "copy")
        } else {
          throw new Error("execCommand unavailable")
        }
      } finally {
        document.body.removeChild(textarea)
      }
    },
    async readText() {
      if (typeof navigator !== "undefined" && navigator.clipboard?.readText) {
        return navigator.clipboard.readText()
      }
      throw new Error("Clipboard read unavailable")
    },
  }
}
