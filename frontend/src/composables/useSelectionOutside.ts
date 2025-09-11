// composables/useSelectionOutside.ts
import { onMounted, onBeforeUnmount } from "vue"
import { useSelectionStore } from "@/stores/selectionStore"

export function useSelectionOutside(extraIgnore: () => HTMLElement | null = () => null) {
  const selection = useSelectionStore()

  function handleClickOutside(e: MouseEvent) {
    const target = e.target as HTMLElement

    // if click inside special card → keep selection
    if (target.closest(".selectable-card")) return
    if (target.closest(".selectable-row")) return


    // if click inside extra element (e.g. rightAside or SlideOver) → keep selection
    const ignore = extraIgnore()
    if (ignore && ignore.contains(target)) return

    selection.clear()
  }

  onMounted(() => document.addEventListener("click", handleClickOutside))
  onBeforeUnmount(() => document.removeEventListener("click", handleClickOutside))
}
