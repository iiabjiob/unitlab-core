// composables/useSelectionOutside.ts
import { onMounted, onBeforeUnmount } from "vue"
import { useSelectionStore } from "@/stores/selectionStore"

export function useSelectionOutside() {
  const selection = useSelectionStore()

  function handleClickOutside(e: MouseEvent) {
    const target = e.target as HTMLElement
    if (selection.shouldKeepSelection(target)) return
    selection.clear()
  }

  onMounted(() => document.addEventListener("click", handleClickOutside))
  onBeforeUnmount(() => document.removeEventListener("click", handleClickOutside))
}
