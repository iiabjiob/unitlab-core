import { computed, ref, watch, type ComputedRef } from "vue"

type SidebarSelectableItem = {
  id: number
}

export type SidebarSelectionAction = "navigate" | "select-only"

export function useSidebarBulkSelection<T extends SidebarSelectableItem>(items: ComputedRef<T[]>) {
  const selectedIds = ref<number[]>([])
  const anchorId = ref<number | null>(null)

  const selectedIdSet = computed(() => new Set(selectedIds.value))
  const selectedCount = computed(() => selectedIds.value.length)

  watch(
    () => items.value.map(item => item.id).join(","),
    () => {
      const availableIds = new Set(items.value.map(item => item.id))
      selectedIds.value = selectedIds.value.filter(id => availableIds.has(id))
      if (anchorId.value !== null && !availableIds.has(anchorId.value)) {
        anchorId.value = selectedIds.value.length
          ? selectedIds.value[selectedIds.value.length - 1]
          : null
      }
    },
  )

  function isSelected(id: number): boolean {
    return selectedIdSet.value.has(id)
  }

  function selectOnly(id: number) {
    selectedIds.value = [id]
    anchorId.value = id
  }

  function clearSelection() {
    selectedIds.value = []
    anchorId.value = null
  }

  function removeIds(ids: number[]) {
    const removedIds = new Set(ids)
    selectedIds.value = selectedIds.value.filter(id => !removedIds.has(id))
    if (anchorId.value !== null && removedIds.has(anchorId.value)) {
      anchorId.value = selectedIds.value.length
        ? selectedIds.value[selectedIds.value.length - 1]
        : null
    }
  }

  function prepareContextSelection(id: number) {
    if (isSelected(id)) {
      return
    }
    selectOnly(id)
  }

  function handleSelection(id: number, event?: MouseEvent | KeyboardEvent): SidebarSelectionAction {
    if (isRangeEvent(event)) {
      selectRange(id)
      return "select-only"
    }

    if (isToggleEvent(event)) {
      toggleSelection(id)
      return "select-only"
    }

    selectOnly(id)
    return "navigate"
  }

  function selectRange(id: number) {
    const itemIds = items.value.map(item => item.id)
    const currentIndex = itemIds.indexOf(id)
    if (currentIndex < 0) {
      return
    }

    const anchorIndex = anchorId.value === null ? -1 : itemIds.indexOf(anchorId.value)
    if (anchorIndex < 0) {
      selectOnly(id)
      return
    }

    const start = Math.min(anchorIndex, currentIndex)
    const end = Math.max(anchorIndex, currentIndex)
    selectedIds.value = itemIds.slice(start, end + 1)
  }

  function toggleSelection(id: number) {
    anchorId.value = id
    selectedIds.value = isSelected(id)
      ? selectedIds.value.filter(selectedId => selectedId !== id)
      : [...selectedIds.value, id]
  }

  return {
    selectedIds,
    selectedIdSet,
    selectedCount,
    isSelected,
    selectOnly,
    clearSelection,
    removeIds,
    prepareContextSelection,
    handleSelection,
  }
}

function isRangeEvent(event?: MouseEvent | KeyboardEvent): boolean {
  return Boolean(event?.shiftKey)
}

function isToggleEvent(event?: MouseEvent | KeyboardEvent): boolean {
  return Boolean(event && (event.ctrlKey || event.metaKey))
}
